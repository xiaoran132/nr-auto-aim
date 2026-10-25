cmake_minimum_required(VERSION 3.20)

project(opencv_learning LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 让 MSVC 按 UTF-8 解析源码，避免中文注释报 C4819 警告
if(MSVC)
    add_compile_options(/utf-8)
endif()

# OpenCV 官方预编译库（vc16 = MSVC 版）
if(Win)
    set(OpenCV_DIR "D:/opencv/opencv/build/x64/vc16/lib")
    find_package(OpenCV REQUIRED)
    
    set(OpenCV_BIN_DIR "D:/opencv/opencv/build/x64/vc16/bin")
#兼容新加的，if-elseif
elseif(linux)
    find_package(OpenCV REQUIRED)
endif()

# 自动把 src/ 和 test/ 目录下的每个 .cpp 编译成一个独立的可执行程序
# CONFIGURE_DEPENDS：新增/删除 .cpp 文件后，下次 Build 会自动重新扫描
file(GLOB EXAMPLE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp"
)

foreach(src ${EXAMPLE_SOURCES})
    get_filename_component(example_name ${src} NAME_WE)
    add_executable(${example_name} ${src})
    target_link_libraries(${example_name} PRIVATE ${OpenCV_LIBS})

    #兼容新加的
    # 编译后复制 OpenCV DLL 到 exe 目录，避免运行时找不到 DLL
    if(WIN32)
    add_custom_command(TARGET ${example_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OpenCV_BIN_DIR}/opencv_world4120.dll"
            "${OpenCV_BIN_DIR}/opencv_world4120d.dll"
            "${OpenCV_BIN_DIR}/opencv_videoio_ffmpeg4120_64.dll"
            "$<TARGET_FILE_DIR:${example_name}>"
             COMMENT "Copying OpenCV DLLs for ${example_name}"
        )
    endif()
    add_custom_command(TARGET ${example_name} POST_BUILD
        # 顺便把 YOLO 模型和类别文件复制到 exe 目录，运行时用相对路径就能找到
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/yolov5s.onnx"
            "${CMAKE_CURRENT_SOURCE_DIR}/coco.names"
            "$<TARGET_FILE_DIR:${example_name}>"
        COMMENT "Copying OpenCV DLLs and model files for ${example_name}"
    )
endforeach()
