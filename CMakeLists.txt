cmake_minimum_required(VERSION 3.20)

project(opencv_learning LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 让 MSVC 按 UTF-8 解析源码，避免中文注释报 C4819 警告
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---- OpenCV 定位 ----
# 换机器时不用改本文件，按优先级覆盖：
#   1) 命令行：cmake -DOpenCV_DIR="D:/opencv/opencv/build/x64/vc16/lib"
#   2) 环境变量：set OpenCV_DIR=...   （Windows）
#   3) 下面的本机默认值
if(NOT OpenCV_DIR AND DEFINED ENV{OpenCV_DIR})
    set(OpenCV_DIR "$ENV{OpenCV_DIR}")
endif()
if(NOT OpenCV_DIR)
    set(OpenCV_DIR "D:/opencv/opencv/build/x64/vc16/lib")
endif()
find_package(OpenCV REQUIRED)

# 从 lib 目录推导同级的 bin 目录（.../build/x64/vc16/bin），不再写死第二处绝对路径
get_filename_component(OpenCV_VC_DIR "${OpenCV_DIR}/.." ABSOLUTE)
set(OpenCV_BIN_DIR "${OpenCV_VC_DIR}/bin")

# 从 OpenCV 版本号推导 DLL 文件名（4.12.0 -> opencv_world4120.dll），
# 换 OpenCV 版本时不用手改这三行
string(REPLACE "." "" OPENCV_VERSION_SHORT "${OpenCV_VERSION}")
set(OPENCV_WORLD_DLL   "${OpenCV_BIN_DIR}/opencv_world${OPENCV_VERSION_SHORT}.dll")
set(OPENCV_WORLD_DLL_D "${OpenCV_BIN_DIR}/opencv_world${OPENCV_VERSION_SHORT}d.dll")
set(OPENCV_FFMPEG_DLL  "${OpenCV_BIN_DIR}/opencv_videoio_ffmpeg${OPENCV_VERSION_SHORT}_64.dll")



#文件目录
file(GLOB EXAMPLE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp"
)

foreach(src ${EXAMPLE_SOURCES})
    get_filename_component(example_name ${src} NAME_WE)
    add_executable(${example_name} ${src})
    target_link_libraries(${example_name} PRIVATE ${OpenCV_LIBS})
    # 编译后复制 OpenCV DLL 到 exe 目录，避免运行时找不到 DLL
    add_custom_command(TARGET ${example_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENCV_WORLD_DLL}"
            "${OPENCV_WORLD_DLL_D}"
            "${OPENCV_FFMPEG_DLL}"
            "$<TARGET_FILE_DIR:${example_name}>"
        # 顺便把 YOLO 模型和类别文件复制到 exe 目录，运行时用相对路径就能找到


        #添加模型文件
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assents/yolov5s.onnx"
            "${CMAKE_CURRENT_SOURCE_DIR}/assents/coco.names"
            "$<{CMAKE_CURRENT_BINARY_DIR}/assents_sp/yolov5.xml"
            "$<{CMAKE_CURRENT_BINARY_DIR}/assents_sp/yolov5.bin"
            #"$<TARGET_FILE_DIR:${example_name}>"
        COMMENT "Copying OpenCV DLLs and model files for ${example_name}"
    )
endforeach()
