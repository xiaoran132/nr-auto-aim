cmake_minimum_required(VERSION 3.20)

project(opencv_learning LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =====================================================================
# 依赖
#   OpenCV   : libopencv-dev（系统库，find_package 自动找到）
#   OpenVINO : 解压安装于 /opt/intel/l_openvino_toolkit_ubuntu22_.../runtime/cmake
#              路径已通过 CMakePresets.json 的 cacheVariables 指定 OpenVINO_DIR。
#              如需改用其他安装位置，可在配置时覆盖：
#                cmake --preset "..." -DOpenVINO_DIR=...
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
    # 把项目根目录作为编译期宏传给每个目标，
    # 源码里用 PROJECT_ROOT_DIR "/相对/路径" 拼出绝对路径，
    # 这样无论从哪个目录启动可执行文件都能找到模型/视频资源。
    target_compile_definitions(${example_name} PRIVATE
        PROJECT_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
endforeach()
