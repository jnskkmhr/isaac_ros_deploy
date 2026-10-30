# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)
project(isaac_common)
find_package(ament_cmake REQUIRED)
install(DIRECTORY include/ DESTINATION include)
ament_export_include_directories(include)
ament_package()
