cmake_minimum_required(VERSION 3.10)
project(rm_vision_cpp)

# 节点层：只做「订阅/发布/参数/可视化」，算法在 nr_vision_* 包里。
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(OpenCV REQUIRED)
find_package(rm_vision_interfaces REQUIRED)
find_package(nr_vision_io REQUIRED)
find_package(nr_vision_detector REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

get_filename_component(NR_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

add_executable(camera_node src/camera_node.cpp)
ament_target_dependencies(camera_node rclcpp sensor_msgs cv_bridge)
target_link_libraries(camera_node nr_vision_io::nr_vision_io ${OpenCV_LIBS})
target_compile_definitions(camera_node PRIVATE NR_REPO_ROOT="${NR_REPO_ROOT}")

add_executable(armor_detector src/detector_node.cpp)
ament_target_dependencies(armor_detector
  rclcpp sensor_msgs geometry_msgs cv_bridge image_transport tf2_geometry_msgs
  rm_vision_interfaces)
target_link_libraries(armor_detector nr_vision_detector::nr_vision_detector ${OpenCV_LIBS})
target_compile_definitions(armor_detector PRIVATE NR_REPO_ROOT="${NR_REPO_ROOT}")

add_executable(armor_tracker src/tracker_node.cpp src/kalman.cpp)
ament_target_dependencies(armor_tracker
  rclcpp geometry_msgs tf2 tf2_ros tf2_geometry_msgs rm_vision_interfaces)
target_link_libraries(armor_tracker Eigen3::Eigen)
target_compile_definitions(armor_tracker PRIVATE NR_REPO_ROOT="${NR_REPO_ROOT}")

install(TARGETS camera_node armor_detector armor_tracker
  DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
