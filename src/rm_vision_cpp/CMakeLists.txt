cmake_minimum_required(VERSION 3.8)
project(rm_vision_cpp)

# 使用 auto，它会自动读取 package.xml 里的依赖
find_package(ament_cmake_auto REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

# 自动寻找依赖（rclcpp, cv_bridge 等）
ament_auto_find_build_dependencies()

# 编译节点
add_executable(video_source src/video_source_node.cpp)
add_executable(armor_detector src/armor_detector_node.cpp src/armor_detect.cpp)
add_executable(armor_tracker src/armor_tracker_node.cpp src/kalman.cpp)

# 链接 OpenCV 和 Eigen（因为 package.xml 里没法精确写 CMake 的库名）
target_link_libraries(video_source ${OpenCV_LIBS})
target_link_libraries(armor_detector ${OpenCV_LIBS} Eigen3::Eigen)
target_link_libraries(armor_tracker Eigen3::Eigen)

# 安装规则
install(TARGETS video_source armor_detector armor_tracker
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

ament_auto_package()
