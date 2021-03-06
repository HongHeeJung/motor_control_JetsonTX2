/* 
lidar control - 1.5 degree division + publish direction Version

      This code was written by Jonathan Wheadon and Ijaas at Plymouth university for module ROCO318
      DATE of last update : 8/11/2018

      2020.07.09 Revise 360 degree laser scanning
      2020.07.20 Change laser scan division to 1.5 degree
      Affine transformation matrix for [approx 1 degree]
      2020.07.21 For Arduino ROS comm.
      2020.07.24 Change Scan data & Vector3f::UnitX() -> [X] axis -> if(direction == 0) CW
      
*/

#include <ros/ros.h>
#include <tf/transform_listener.h> // ROS library
#include <sensor_msgs/PointCloud2.h>
#include <laser_geometry/laser_geometry.h>
#include <std_msgs/Int16.h>
#include <iostream>
#include <pcl/io/pcd_io.h>
#include <pcl/common/io.h>
#include <pcl/point_cloud.h>
#include <pcl/common/transforms.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl_ros/transforms.h>
#include <pcl_ros/impl/transforms.hpp>

using namespace Eigen; //space transformation 
using namespace ros;

// Variable to count how many 2d scans have been taken
int ScanNo = 0; // 180도를 1도씩 나눔 -> ScanNO: counter
int direction = 0;
const float space_radian = 0.0261799;  // degree to radian 변환값을 바꿔서 각도를 원하는 값으로 나눔
const int degree_offset = 0;

// Variables store the previous cloud and fully assembled cloud
pcl::PointCloud<pcl::PointXYZ> oldcloud;
pcl::PointCloud<pcl::PointXYZ> assembledCloud;
sensor_msgs::PointCloud2 OutPutCloud;

// Create ScanAssembler class
class ScanAssembler {
    public:
        ScanAssembler();
        void scanCallback(const sensor_msgs::LaserScan::ConstPtr& scan);
    private:
        NodeHandle node_;
        tf::TransformListener tfListener_;
        laser_geometry::LaserProjection projector_;

        Subscriber scan_sub_;
        Publisher full_point_cloud_publisher_;
        Publisher motor_control_publisher_;
};

ScanAssembler::ScanAssembler(){
    scan_sub_ = node_.subscribe<sensor_msgs::LaserScan> ("/scan", 1000, &ScanAssembler::scanCallback, this);
    // RVIZ 상에 나타나는 point cloud
    full_point_cloud_publisher_ = node_.advertise<sensor_msgs::PointCloud2> ("/fullcloud", 1000, false);
    motor_control_publisher_ = node_.advertise<std_msgs::Int16> ("/control", 1, false);
}

void ScanAssembler::scanCallback(const sensor_msgs::LaserScan::ConstPtr& scan){
    // Create control variable    
    std_msgs::Int16 Control;

    // Convert laser scan to point cloud
    sensor_msgs::PointCloud2 cloud;
    // Transform a sensor_msgs::LaserScan into a sensor_msgs::PointCloud2 in target frame
    projector_.transformLaserScanToPointCloud("/laser", *scan, cloud, tfListener_);

    // Convert ROS point cloud into PCL point cloud
    pcl::PointCloud<pcl::PointXYZ> TempCloud;
    pcl::fromROSMsg(cloud, TempCloud);

    // Initialise PCL point cloud for storing rotated cloud
    pcl::PointCloud<pcl::PointXYZ>  RotatedCloud;

    // Create Affine transformation matrix for 0.0174533 radians around Z axis (approx 1 degree) of rotation with no translation.
    Affine3f RotateMatrix = Affine3f::Identity();
    RotateMatrix.translation() << 0.0, 0.0, 0.0;

    // Rotate matrix has offset of 90 degrees to set start pos
    if(direction == 0){
        RotateMatrix.rotate (AngleAxisf (((ScanNo - degree_offset) * space_radian), Vector3f::UnitX()));
    } else {
        RotateMatrix.rotate (AngleAxisf (((ScanNo + degree_offset) * space_radian), Vector3f::UnitX()));
    }

    // Rotate Tempcloud by rotation matrix timesed by the scan number, AKA how many scan have been taken.
    pcl::transformPointCloud (TempCloud, RotatedCloud, RotateMatrix);

     
    //  If this is first scan save rotated cloud as old cloud
    //  if this is not first scan concatanate rotated cloud and oldcloud
    //  together save new cloud as old cloud and output to rviz
    Control.data = direction;
    motor_control_publisher_.publish(Control); // Start motor

	if(ScanNo > 0){
	    assembledCloud = oldcloud + RotatedCloud;
	    pcl::copyPointCloud(assembledCloud, oldcloud);
	    ScanNo += 1.5;
	    if(ScanNo > 119){
		ScanNo = 0;
		direction = (direction+1)%2;
	    }
	} else {
	    oldcloud = RotatedCloud;
	    assembledCloud = RotatedCloud;
	    ScanNo += 1.5;
	}

    // Convert Assembled cloud to ROS cloud and publish all
    pcl::toROSMsg(assembledCloud, OutPutCloud);
    full_point_cloud_publisher_.publish(OutPutCloud);
    Control.data = direction;
    motor_control_publisher_.publish(Control); //Arduino가 Subscribe 방향

    // Return to spin until next laser scan taken
    return;
}

int main(int argc, char** argv)
{
    init(argc, argv, "ScanAssembler");
    ScanAssembler assembler;
    spin();

    return 0;
}
