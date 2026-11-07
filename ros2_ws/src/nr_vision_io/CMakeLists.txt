cmake_minimum_required(VERSION 3.10)
project(nr_vision_io)

# 相机驱动包装：不复制 main 的 io/ 源码，直接编译仓库根下的 io/，
# 保证「相机驱动只有一份实现」，与 main 行为一致。
find_package(ament_cmake REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(OpenCV REQUIRED)
find_package(yaml-cpp REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

get_filename_component(NR_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
message(STATUS "nr_vision_io: NR_REPO_ROOT = ${NR_REPO_ROOT}")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  set(MVS_ARCH arm64)
else()
  set(MVS_ARCH amd64)
endif()
set(MVS_DIR "${NR_REPO_ROOT}/io/hikrobot")
set(MVS_LIB_DIR "${MVS_DIR}/lib/${MVS_ARCH}")
set(MVS_LIB "${MVS_LIB_DIR}/libMvCameraControl.so")
if(NOT EXISTS "${MVS_LIB}")
  message(FATAL_ERROR "找不到海康 MVS SDK: ${MVS_LIB}（请确认仓库内 io/hikrobot/lib/${MVS_ARCH} 完整）")
endif()

add_library(nr_vision_io SHARED
  src/camera_driver.cpp
  ${NR_REPO_ROOT}/io/camera.cpp
  ${NR_REPO_ROOT}/io/usbcamera/usbcamera.cpp
  ${NR_REPO_ROOT}/io/hikrobot/hikrobot.cpp)

target_include_directories(nr_vision_io
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${NR_REPO_ROOT}
    ${NR_REPO_ROOT}/io
    ${MVS_DIR}/include)

target_link_directories(nr_vision_io PUBLIC ${MVS_LIB_DIR})
target_link_libraries(nr_vision_io
  PUBLIC ${OpenCV_LIBS} yaml-cpp sensor_msgs::sensor_msgs
  PRIVATE MvCameraControl)
target_compile_definitions(nr_vision_io PRIVATE NR_REPO_ROOT="${NR_REPO_ROOT}")
set_target_properties(nr_vision_io PROPERTIES
  BUILD_RPATH "${MVS_LIB_DIR}"
  INSTALL_RPATH "${MVS_LIB_DIR}")

install(TARGETS nr_vision_io
  EXPORT export_nr_vision_io
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

ament_export_include_directories(include)
ament_export_targets(export_nr_vision_io HAS_LIBRARY_TARGET)
ament_export_dependencies(sensor_msgs OpenCV yaml-cpp)
ament_package()
