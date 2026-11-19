cmake_minimum_required(VERSION 3.10)
project(nr_vision_detector)

# 纯算法库（YOLO + 关键点排序 + PnP），不依赖 rclcpp；头文件不暴露 OpenVINO。
find_package(ament_cmake REQUIRED)
find_package(OpenCV REQUIRED)
find_package(OpenVINO REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

add_library(nr_vision_detector SHARED
  src/yolo_detector.cpp
  src/armor_pose.cpp)

target_include_directories(nr_vision_detector
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(nr_vision_detector
  PUBLIC ${OpenCV_LIBS}
  PRIVATE openvino::runtime)



install(TARGETS nr_vision_detector
  EXPORT export_nr_vision_detector
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

ament_export_include_directories(include)
ament_export_targets(export_nr_vision_detector HAS_LIBRARY_TARGET)
ament_export_dependencies(OpenCV)
ament_package()
