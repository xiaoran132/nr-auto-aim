cmake_minimum_required(VERSION 3.20)

project(nr_auto_aim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# =====================================================================
# 依赖
#   OpenCV    : libopencv-dev
#   OpenVINO  : libopenvino-dev（find_package 自动定位系统安装路径）
#   yaml-cpp  : libyaml-cpp-dev（读取 configs/*.yaml）
# =====================================================================
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV ${OpenCV_VERSION}: ${OpenCV_DIR}")

# opcv_yolo5 使用 cv::dnn::readNetFromModelOptimizer，要求 OpenCV 编译时
# 开启 -DWITH_OPENVINO=ON。发行版包通常是 OFF，这里只告警、不阻断编译。
find_library(_opencv_dnn_lib NAMES opencv_dnn)
if(_opencv_dnn_lib)
    execute_process(
        COMMAND ldd "${_opencv_dnn_lib}"
        OUTPUT_VARIABLE _opencv_dnn_ldd
        ERROR_QUIET)
    if(NOT _opencv_dnn_ldd MATCHES "openvino")
        message(WARNING
            "OpenCV（${_opencv_dnn_lib}）未链接 OpenVINO 后端，"
            "opcv_yolo5 运行时会报 "
            "Backend (plugin) is not available: 'openvino'。")
    endif()
endif()

find_package(OpenVINO REQUIRED)
message(STATUS
    "OpenVINO ${OpenVINO_VERSION_MAJOR}.${OpenVINO_VERSION_MINOR}.${OpenVINO_VERSION_PATCH}: "
    "${OpenVINO_DIR}")

find_package(yaml-cpp REQUIRED)

# =====================================================================
# 海康 MVS 机器视觉 SDK（位于 io/hikrobot/）
# =====================================================================
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(NR_HIKROBOT_LIB_DIR "${PROJECT_SOURCE_DIR}/io/hikrobot/lib/amd64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(NR_HIKROBOT_LIB_DIR "${PROJECT_SOURCE_DIR}/io/hikrobot/lib/arm64")
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()
message(STATUS "MVS SDK: ${NR_HIKROBOT_LIB_DIR}")

add_subdirectory(tools)
add_subdirectory(io)

# =====================================================================
# 可执行文件（src/ 与 tests/ 各一个 .cpp 对应一个 target）
# =====================================================================
add_executable(04_check_model src/04_check_model.cpp)
add_executable(05             src/05.cpp)
add_executable(camera_test    tests/camera_test.cpp)
add_executable(camera_viewer  tests/camera_viewer.cpp)
add_executable(opcv_yolo5     src/opcv_yolo5.cpp)
add_executable(opcv_yolo8     src/opcv_yolo8.cpp)
add_executable(opcv_yolo11    src/opcv_yolo11.cpp)
add_executable(video_yolo     src/video_yolo.cpp)
add_executable(camera_yolo    src/camera_yolo.cpp)
add_executable(dir_test       tests/dir_test.cpp)
add_executable(yolo_test      tests/yolo_test.cpp)

target_link_libraries(04_check_model PRIVATE ${OpenCV_LIBS})
target_link_libraries(05             PRIVATE ${OpenCV_LIBS})
target_link_libraries(camera_test    PRIVATE io ${OpenCV_LIBS})
target_link_libraries(camera_viewer  PRIVATE io ${OpenCV_LIBS})
target_link_libraries(opcv_yolo5     PRIVATE ${OpenCV_LIBS})
target_link_libraries(opcv_yolo8     PRIVATE ${OpenCV_LIBS} openvino::runtime)
target_link_libraries(opcv_yolo11    PRIVATE io ${OpenCV_LIBS} openvino::runtime)
target_link_libraries(video_yolo     PRIVATE ${OpenCV_LIBS} openvino::runtime)
target_link_libraries(camera_yolo    PRIVATE io ${OpenCV_LIBS} openvino::runtime)
target_link_libraries(dir_test       PRIVATE ${OpenCV_LIBS})
target_link_libraries(yolo_test      PRIVATE ${OpenCV_LIBS})

# 所有目标统一：
#   PROJECT_ROOT_DIR —— 源码里拼出项目根目录下的资源路径，与启动目录无关
#   BUILD_RPATH      —— 运行时找到 libMvCameraControl.so，无需 LD_LIBRARY_PATH
set(NR_EXECUTABLES
    04_check_model
    05
    camera_test
    camera_viewer
    opcv_yolo5
    opcv_yolo8
    opcv_yolo11
    video_yolo
    camera_yolo
    dir_test
    yolo_test)

foreach(target IN LISTS NR_EXECUTABLES)
    target_compile_definitions(${target} PRIVATE
        PROJECT_ROOT_DIR="${PROJECT_SOURCE_DIR}")
    set_target_properties(${target} PROPERTIES
        BUILD_RPATH "${NR_HIKROBOT_LIB_DIR}")
endforeach()
