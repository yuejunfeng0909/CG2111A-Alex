#!/usr/bin/env bash

CATKIN_SHELL=bash

export ROS_IP=172.20.10.3
export ROS_MASTER_URI=http://172.20.10.3:11311

source ~/Desktop/slam/devel/setup.bash
cd ~/Desktop/slam
roslaunch rplidar_ros rplidar.launch