cmake_minimum_required(VERSION 3.16)
project(g1_description)
find_package(ament_cmake REQUIRED)
install(DIRECTORY urdf mjcf meshes DESTINATION share/${PROJECT_NAME})
install(FILES LICENSE LICENSE.unitree DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_pytest REQUIRED)
  ament_add_pytest_test(test_actuator_limits test/test_actuator_limits.py)
endif()

ament_package()
