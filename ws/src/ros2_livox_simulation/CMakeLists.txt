cmake_minimum_required(VERSION 3.5)
project(ros2_livox_simulation)

add_compile_options(-std=c++17)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ===============================
# find dependencies
# ===============================
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(gazebo_dev REQUIRED)
find_package(gazebo_ros REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(livox_ros_driver2 REQUIRED)
find_package(Boost REQUIRED COMPONENTS chrono)
find_package(Eigen3 REQUIRED)
find_package(nav_msgs REQUIRED)

# ===============================
# include & link directories
# ===============================
include_directories(
  include
)
include_directories(${GAZEBO_INCLUDE_DIRS})
include_directories(/usr/include/gazebo-11/gazebo)
link_directories(${GAZEBO_LIBRARY_DIRS})

# ===============================
# install include
# ===============================
install(DIRECTORY include/
        DESTINATION include
)

# ===============================
# Gazebo plugin library (Livox)
# ===============================
add_library(ros2_livox SHARED
  src/livox_points_plugin.cpp
  src/livox_ode_multiray_shape.cpp
)

target_link_libraries(ros2_livox
  ${GAZEBO_LIBRARIES}
  RayPlugin
  GpuRayPlugin
  libprotobuf.so.9
  libboost_chrono.so.1.71.0
)

ament_target_dependencies(ros2_livox
  rclcpp
  std_msgs
  sensor_msgs
  geometry_msgs
  gazebo_dev
  gazebo_ros
  tf2_ros
  livox_ros_driver2
)

target_include_directories(ros2_livox PRIVATE include)

install(TARGETS ros2_livox
  LIBRARY DESTINATION lib
)

# ===============================
# Gazebo plugin library (TF publisher)
# ===============================
add_library(tf_pub SHARED
  src/tf_pub.cpp
)

target_link_libraries(tf_pub
  ${GAZEBO_LIBRARIES}
)

ament_target_dependencies(tf_pub
  rclcpp
  geometry_msgs
  tf2_ros
  gazebo_dev
  gazebo_ros
  nav_msgs
)

target_include_directories(tf_pub PRIVATE include)

install(TARGETS tf_pub
  LIBRARY DESTINATION lib
)

# ===============================
# FixCloudTime node (C++)
# ===============================
add_executable(fix_cloud_time
  src/fix_cloud_time.cpp
)

ament_target_dependencies(fix_cloud_time
  rclcpp
  sensor_msgs
)

install(TARGETS
  fix_cloud_time
  DESTINATION lib/${PROJECT_NAME}
)
target_include_directories(fix_cloud_time
  PRIVATE
    ${EIGEN3_INCLUDE_DIRS}
)

# ===============================
# FixImuTime node (C++)
# ===============================
add_executable(fix_imu_time
  src/fix_imu_time.cpp
)

ament_target_dependencies(fix_imu_time
  rclcpp
  sensor_msgs
  geometry_msgs
)

install(TARGETS
  fix_imu_time
  DESTINATION lib/${PROJECT_NAME}
)

# ===============================
# testing
# ===============================
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

# ===============================
# export libraries
# ===============================
ament_export_libraries(
  ros2_livox
)

# ===============================
# install CSV files
# ===============================
set(CSV_FILES
  scan_mode/mid40.csv
  scan_mode/mid70.csv
  scan_mode/mid360.csv
  scan_mode/tele.csv
  scan_mode/horizon.csv
  scan_mode/HAP.csv
  scan_mode/avia.csv
)

foreach(CSV_FILE ${CSV_FILES})
  configure_file(${CSV_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${CSV_FILE} COPYONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CSV_FILE}
    DESTINATION share/${PROJECT_NAME}/scan_mode
  )
endforeach()

# ===============================
# install URDF files
# ===============================
set(URDF_FILES
  urdf/mid40.xacro
  urdf/mid70.xacro
  urdf/mid360.xacro
  urdf/tele.xacro
  urdf/horizon.xacro
  urdf/HAP.xacro
  urdf/avia.xacro
  urdf/robot_sim.xacro
)

foreach(URDF_FILE ${URDF_FILES})
  configure_file(${URDF_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${URDF_FILE} COPYONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${URDF_FILE}
    DESTINATION share/${PROJECT_NAME}/urdf
  )
endforeach()

install(DIRECTORY launch urdf config world models
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
