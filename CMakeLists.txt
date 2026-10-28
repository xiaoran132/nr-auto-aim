cmake_minimum_required(VERSION 3.20)

project(opencv_learning LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =====================================================================
# 依赖（均使用本机系统库，不需要手动指定路径）
#   OpenCV   : libopencv-dev
#   OpenVINO : 系统安装的 runtime（位于 /usr/lib/cmake/openvino*）
# 如需改用其他安装位置，可在配置时覆盖：
#   cmake -DOpenCV_DIR=... -DOpenVINO_DIR=...
# =====================================================================
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV ${OpenCV_VERSION}: ${OpenCV_DIR}")

# opcv_yolo5 / opcv_yolo8 调用 cv::dnn::readNetFromModelOptimizer，
# 要求 OpenCV 编译时开启 -DWITH_OPENVINO=ON。发行版包通常是 OFF，
# 这里只告警、不阻断其余目标编译。
find_library(_opencv_dnn_lib NAMES opencv_dnn)
if(_opencv_dnn_lib)
    execute_process(
        COMMAND ldd "${_opencv_dnn_lib}"
        OUTPUT_VARIABLE _opencv_dnn_ldd
        ERROR_QUIET)
    if(NOT _opencv_dnn_ldd MATCHES "openvino")
        message(WARNING
            "OpenCV（${_opencv_dnn_lib}）未链接 OpenVINO 后端，"
            "opcv_yolo5 / opcv_yolo8 运行时会报 "
            "Backend (plugin) is not available: 'openvino'。")
    endif()
endif()

# OpenVINO C++ API：yolo11 等模型直接用它推理，
# 不经过 OpenCV dnn 的 OpenVINO 后端。
find_package(OpenVINO REQUIRED)
message(STATUS "OpenVINO ${OpenVINO_VERSION_MAJOR}.${OpenVINO_VERSION_MINOR}.${OpenVINO_VERSION_PATCH}: ${OpenVINO_DIR}")

# =====================================================================
# 可执行文件：src/ 和 test/ 下的每个 .cpp 编译成一个目标
# =====================================================================
file(GLOB EXAMPLE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp"
)

foreach(src ${EXAMPLE_SOURCES})
    get_filename_component(example_name ${src} NAME_WE)
    add_executable(${example_name} ${src})
    target_link_libraries(${example_name} PRIVATE ${OpenCV_LIBS} openvino::runtime)
endforeach()
