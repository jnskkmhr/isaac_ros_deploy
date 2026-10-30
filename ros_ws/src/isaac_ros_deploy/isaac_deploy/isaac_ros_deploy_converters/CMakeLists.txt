# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)
project(isaac_ros_deploy_converters)

# C++20 to match isaac_deploy_core.
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force RPATH instead of RUNPATH for better plugin loading.
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
# Disable RUNPATH (use RPATH instead).
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--disable-new-dtags")

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-missing-field-initializers)
endif()

# Find dependencies.
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)

# Find isaac_deploy_core first - it exports LibTorch and ONNX Runtime for us.
find_package(isaac_deploy_core REQUIRED)
find_package(isaac_common REQUIRED)

find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(isaac_ros_deploy_interfaces REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(yaml-cpp REQUIRED)

# LibTorch is provided by isaac_deploy_core via isaac_deploy_core-extras.cmake.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

# =============================================================================
# Converters Library (message-to-tensor, shared with isaac_ros_deploy_ros2_control)
# =============================================================================

# nvtx_scope.hpp is included unconditionally and compiles to no-ops without USE_NVTX, so its
# include path is required in both configurations; only the CUDA path is gated.
include_directories(${isaac_common_INCLUDE_DIRS})
option(USE_NVTX "Enable NVTX markers for improved profiling (if available)" ON)
if(USE_NVTX)
  add_definitions(-DUSE_NVTX)
  include_directories(${CUDAToolkit_INCLUDE_DIRS})
endif()

add_library(converters SHARED
  src/converters/message_to_tensor_converters.cpp
)
target_include_directories(converters PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(converters
  rclcpp
  isaac_ros_deploy_interfaces
  sensor_msgs
  geometry_msgs
  nav_msgs
)
target_link_libraries(converters
  isaac_deploy_core::types
  ${TORCH_LIBRARIES}
)
set_target_properties(converters PROPERTIES
  INSTALL_RPATH "${TORCH_INSTALL_PREFIX}/lib"
  BUILD_RPATH "${TORCH_INSTALL_PREFIX}/lib"
)

install(TARGETS converters EXPORT ${PROJECT_NAME}_targets
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
ament_export_targets(${PROJECT_NAME}_targets HAS_LIBRARY_TARGET)
ament_export_dependencies(isaac_deploy_core rclcpp isaac_ros_deploy_interfaces sensor_msgs geometry_msgs nav_msgs)
ament_package()
