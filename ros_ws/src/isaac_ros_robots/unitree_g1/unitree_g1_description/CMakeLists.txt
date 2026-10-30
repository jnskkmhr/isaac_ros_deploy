# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)
project(unitree_g1_description)
find_package(ament_cmake REQUIRED)
# g1_description supplies the robot assets. Retain the official hardware interface.
install(FILES urdf/g1_real_ros2_control.urdf.xacro DESTINATION share/${PROJECT_NAME}/urdf)
ament_package()
