cmake_minimum_required(VERSION 3.10)
project(rm_vision_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake_auto REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

ament_auto_find_build_dependencies()

# ---------------- 纯算法库（不依赖 ROS） ----------------
add_library(rm_vision_core SHARED
  src/armor_detect.cpp
  src/kalman.cpp
)

target_include_directories(rm_vision_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(rm_vision_core PUBLIC
  ${OpenCV_LIBS}
  Eigen3::Eigen
)

# ---------------- 节点 ----------------
ament_auto_add_executable(video_source src/video_source_node.cpp)
target_link_libraries(video_source ${OpenCV_LIBS})

ament_auto_add_executable(armor_detector src/armor_detector_node.cpp)
target_link_libraries(armor_detector rm_vision_core ${OpenCV_LIBS})

ament_auto_add_executable(armor_tracker src/armor_tracker_node.cpp)
target_link_libraries(armor_tracker rm_vision_core)

# ---------------- 安装 ----------------
install(
  TARGETS rm_vision_core
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(
  DIRECTORY include/
  DESTINATION include
)

ament_auto_package(
  INSTALL_TO_SHARE
  launch
  config
)
