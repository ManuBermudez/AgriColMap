#include "../pointcloud_handler/pointcloud_aligner.h"
#include "../visualizer/visualizer.h"

using namespace std;

int main(int argc, char **argv) {


    PointCloudAligner pclAligner;

    // Reading input Clouds
    pclAligner.loadFixedCloudFromDisk("point_cloud", "20180524-mavic-uav-20m-sugarbeet-eschikon", "cloud" );
    pclAligner.loadMovingCloudFromDisk("point_cloud", "20180524-mavic-ugv-sugarbeet-eschikon-row3_20m", "row3_cloud", "cloud", Vector2(1, 1) );
    pclAligner.loadMovingCloudFromDisk("point_cloud", "20180524-mavic-ugv-sugarbeet-eschikon-row4_20m", "row4_cloud", "cloud", Vector2(1, 1) );
    pclAligner.loadMovingCloudFromDisk("point_cloud", "20180524-mavic-ugv-sugarbeet-eschikon-row5_20m", "row5_cloud", "cloud", Vector2(1, 1) );

    // Computing Filtered ExG Clouds
    pclAligner.getPointCloud("row3_cloud")->computeFilteredPcl( Vector3i(255,0,0) );
    pclAligner.getPointCloud("row4_cloud")->computeFilteredPcl( Vector3i(0,255,0) );
    pclAligner.getPointCloud("row5_cloud")->computeFilteredPcl( Vector3i(0,0,255) );

    // Transforming Clouds with Grount Truth
    pclAligner.GroundTruthTransformPointCloud("row3_cloud");
    pclAligner.GroundTruthTransformPointCloud("row4_cloud");
    pclAligner.GroundTruthTransformPointCloud("row5_cloud");

    // Enhance Brightness for the UAV Cloud
    int brightness = 100;
    pclAligner.getPointCloud("cloud")->BrightnessEnhancement(brightness);

    // Visualize
    PointCloudViz viz;
    viz.setViewerBackground(255,255,255);
    viz.showCloud( pclAligner.getPointCloud("row3_cloud")->getPointCloudFiltered(), "row3_cloud" );
    viz.showCloud( pclAligner.getPointCloud("row4_cloud")->getPointCloudFiltered(), "row4_cloud" );
    viz.showCloud( pclAligner.getPointCloud("row5_cloud")->getPointCloudFiltered(), "row5_cloud" );
    viz.showTransparentCloud( pclAligner.getPointCloud("cloud")->getPointCloud(), "cloud" );
    viz.spingUntilDeath();

    return 0;
}
