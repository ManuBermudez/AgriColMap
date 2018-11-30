#include "pointcloud_handler.h"

void PointCloudHandler::loadCloud(const std::string &cloud_name, const std::string &cloud_path, const std::string &cloud_key){

    // Check for the fixed cloud path
    string input_pcl_str = _package_path + "/maps/" + cloud_path + "/" + cloud_name + ".ply";
    ifstream input_pcl( input_pcl_str );
    if(!input_pcl)
        ExitWithErrorMsg("File Does Not Exist: " + input_pcl_str);

    PCLPointCloudXYZRGB::Ptr _pcl_data( new PCLPointCloudXYZRGB() );
    pcl::io::loadPLYFile<PCLptXYZRGB> ( input_pcl_str, *_pcl_data);

    //_pclMap[cloud_key]->loadFromPcl(_pcl_data);
    pclMap.emplace( cloud_key, _pcl_data );
    cerr << FGRN("Correctly Imported: ") << input_pcl_str << " " << pclMap[cloud_key]->points.size() << " Points" << "\n";

    // Reading the fixed Offset
    string fixed_offset;
    ifstream fixed_offset_file( _package_path + "/maps/" + cloud_path + "/" + "offset.xyz" );
    getline(fixed_offset_file, fixed_offset);
    vector<double> fixed_utm_vec = vectorFromString(fixed_offset);
    initGuessTMap.emplace(cloud_key, Vector3d(fixed_utm_vec[0], fixed_utm_vec[1], fixed_utm_vec[2]));
    initGuessQMap.emplace(cloud_key, Vector3(fixed_utm_vec[3], fixed_utm_vec[4], fixed_utm_vec[5]));

    if(_verbosity){
        cerr << FYEL("Initial Guess T Correctly Imported: ") << initGuessTMap[cloud_key].transpose() << "\n";
        cerr << FYEL("Initial Guess Q Correctly Imported: ") << initGuessQMap[cloud_key].transpose() << "\n" << "\n";
    }

    Vector3 mean(0.f, 0.f, 0.f);
    for(unsigned int i = 0; i < pclMap[cloud_key]->points.size(); ++i)
        mean += pclMap[cloud_key]->points[i].getVector3fMap();
    mean /= pclMap[cloud_key]->points.size();

    Transform norm_T( Transform::Identity() );
    norm_T.translation() << -mean;
    pcl::transformPointCloud(*pclMap[cloud_key], *pclMap[cloud_key], norm_T);
}

void PointCloudHandler::planeNormalization(const std::string& cloud_key){

    pcl::ModelCoefficients::Ptr coefficients (new pcl::ModelCoefficients);
    pcl::PointIndices::Ptr inliers (new pcl::PointIndices);
    PCLsegmentationXYZRGB seg;

    seg.setOptimizeCoefficients (true);
    seg.setModelType (pcl::SACMODEL_PLANE);
    seg.setMethodType (pcl::SAC_RANSAC);
    seg.setDistanceThreshold (0.01);

    //seg.setInputCloud (temp_xyz_pcl);
    seg.setInputCloud (pclMap[cloud_key]);
    seg.segment (*inliers, *coefficients);

    Vector3 ez;
    if( coefficients->values[2] > 0)
        ez = Vector3(coefficients->values[0], coefficients->values[1], coefficients->values[2]);
    else if( coefficients->values[2] < 0 )
        ez = Vector3(-coefficients->values[0], -coefficients->values[1], -coefficients->values[2]);

    Vector3 ez_B = ez / ez.norm();

    float yaw_des = initGuessQMap[cloud_key](2)*(3.14/180); //init_guess_q(2)*(3.14/180);
    Vector3 ey_C( -sin(yaw_des), cos(yaw_des), 0 );

    Vector3 ex_B = ey_C.cross( ez_B ) / ( ey_C.cross( ez_B ) ).norm();
    Vector3 ey_B = ez_B.cross( ex_B ) / ( ez_B.cross( ex_B ) ).norm();

    Matrix3 A; A.col(0) = ex_B; A.col(1) = ey_B; A.col(2) = ez_B;

    Transform R = Transform::Identity();
    R.rotate(A.transpose());
    R.translation() << Vector3(0,0,coefficients->values[3]);

    R.translation() << Vector3(0,0,coefficients->values[3]);
    //transformPointCloud(R);
    pcl::transformPointCloud(*pclMap[cloud_key], *pclMap[cloud_key], R);

    return;
}

void PointCloudHandler::initFromYaml(const std::string& yaml_file){

    cerr << FBLU("Initializing from:") << " " << yaml_file << "\n" << "\n";
    YAML::Node configuration = YAML::LoadFile(yaml_file);

    // Loading info for fixed input cloud from YAML
    _fixed_pcl_path = configuration["input_clouds"]["cloud_fixed_path"].as<string>();
    _fixed_pcl = configuration["input_clouds"]["cloud_fixed_name"].as<string>();

    // Check for the fixed cloud path
    string input_pcl = _package_path + "/maps/" + _fixed_pcl_path + "/" + _fixed_pcl + ".ply";
    ifstream in_fixed_pcl( input_pcl );
    if(!in_fixed_pcl)
        ExitWithErrorMsg("File Does Not Exist: " + input_pcl);

    // Loading info for moving input cloud from YAML
    _moving_pcl_path = configuration["input_clouds"]["cloud_moving_path"].as<string>();
    _moving_pcl = configuration["input_clouds"]["cloud_moving_name"].as<string>();

    // Check for the moving cloud path
    input_pcl = _package_path + "/maps/" + _moving_pcl_path + "/" + _moving_pcl + ".ply";
    ifstream mov_fixed_pcl( input_pcl );
    if(!mov_fixed_pcl)
        ExitWithErrorMsg("File Does Not Exist: " + input_pcl);

    // Reading mov_scale
    vector<float> mov_scale_vec = configuration["input_clouds"]["relative_scale"].as<std::vector<float>>();
    _init_mov_scale << mov_scale_vec[0], mov_scale_vec[1];

    _max_iter_num = configuration["aligner_params"]["max_iter_number"].as<int>();
    _verbosity = configuration["aligner_params"]["verbosity"].as<bool>();
    _storeDenseOptFlw = configuration["aligner_params"]["store_dense_optical_flow"].as<bool>();
    _showDOFCorrespondences = configuration["aligner_params"]["show_dpf_correspondences"].as<bool>();
    _dense_optical_flow_step = configuration["aligner_params"]["dense_optical_flow_step"].as<int>();
    _downsampling_rate = configuration["aligner_params"]["downsampling_rate"].as<float>();
    _search_radius = configuration["aligner_params"]["search_radius"].as<float>();
    _useVisualFeatures = configuration["aligner_params"]["use_visual_features"].as<bool>();
    _useGeometricFeatures = configuration["aligner_params"]["use_geometric_features"].as<bool>();

}

void PointCloudHandler::loadFromDisk(const std::string& fixed_cloud_key, const std::string& moving_cloud_key){

    loadCloud(_moving_pcl, _moving_pcl_path, moving_cloud_key);
    loadCloud(_fixed_pcl, _fixed_pcl_path, fixed_cloud_key);

    string ground_truth_tf_path = _package_path + "/params/output/" + _moving_pcl_path + "_AffineGroundTruth.txt";
    ifstream mov_fixed_pcl( ground_truth_tf_path ); bool groundTruth = false;
    if(mov_fixed_pcl) {
        string affine_gt_tf; groundTruth = true;
        getline(mov_fixed_pcl, affine_gt_tf);
        Matrix3 _Rgt; Vector3 _tgt; Vector2 _scl;
        AffineTransformFromString(affine_gt_tf, _Rgt, _tgt, _scl);
        GTtfMap.emplace( moving_cloud_key, boost::shared_ptr<GroundTruth>(new GroundTruth(_Rgt, _tgt, _scl)));
    }

    if(_verbosity && groundTruth){
        cerr << FYEL("Ground Truth Affine Matrix: ") << "\n" << GTtfMap[moving_cloud_key]->_Rgt << "\n";
        cerr << FYEL("Ground Truth Translation: ") << GTtfMap[moving_cloud_key]->_tgt.transpose() << "\n";
        cerr << FYEL("Ground Truth Relative Scale: ") << GTtfMap[moving_cloud_key]->_rel_scl.transpose() << "\n" << "\n";
    }

}

void PointCloudHandler::scalePointCloud(const Vector2 &scale_factors, const string &cloud_to_scale){

    Transform scaling_tf = Transform::Identity();
    scaling_tf(0,0) *= scale_factors(0);
    scaling_tf(1,1) *= scale_factors(1);
    pcl::transformPointCloud(*pclMap[cloud_to_scale], *pclMap[cloud_to_scale], scaling_tf);

}
