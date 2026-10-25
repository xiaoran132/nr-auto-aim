cmake_minimum_required(VERSION 3.20)

project(opencv_learning LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 让 MSVC 按 UTF-8 解析源码，避免中文注释报 C4819 警告
if(MSVC)
    add_compile_options(/utf-8)
endif()

# OpenCV 官方预编译库（vc16 = MSVC 版）
set(OpenCV_DIR "D:/opencv/opencv/build/x64/vc16/lib")
find_package(OpenCV REQUIRED)

set(OpenCV_BIN_DIR "D:/opencv/opencv/build/x64/vc16/bin")

# ---- OpenVINO Runtime（重新编译带 OpenVINO 后端的 OpenCV 后，运行时需要）----
# 换机器/换 OpenVINO 版本时改这一个目录即可
set(OPENVINO_SDK_ROOT "C:/dev/openvino_sdk/w_openvino_toolkit_windows_2024.6.0.17404.4c0f47d2335_x86_64")
set(OPENVINO_RUNTIME_BIN "${OPENVINO_SDK_ROOT}/runtime/bin/intel64/Release")
set(OPENVINO_TBB_BIN     "${OPENVINO_SDK_ROOT}/runtime/3rdparty/tbb/bin")

# ---- OpenVINO C++ API ----
# 用于直接调用 OpenVINO 推理（绕过 OpenCV dnn 的 OpenVINO 后端）。
# 原因：YOLO11 模型（带 C2PSA 注意力 + Detect 头动态 shape 算子）会让
# OpenCV dnn 的 OpenVINO 后端在 readNetFromModelOptimizer 阶段栈溢出崩溃（0xC0000409），
# 而 OpenVINO 2024.6 运行时本身能正常加载并推理该模型。
if(NOT OpenVINO_DIR)
    set(OpenVINO_DIR "${OPENVINO_SDK_ROOT}/runtime/cmake")
endif()
find_package(OpenVINO REQUIRED)



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
    if(WIN32)
    add_custom_command(TARGET ${example_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENCV_WORLD_DLL}"
            "${OPENCV_FFMPEG_DLL}"
            "$<TARGET_FILE_DIR:${example_name}>"
        # 顺便把 YOLO 模型和类别文件复制到 exe 目录，运行时用相对路径就能找到
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENVINO_RUNTIME_BIN}/openvino.dll"
            "${OPENVINO_RUNTIME_BIN}/openvino_intel_cpu_plugin.dll"
            "${OPENVINO_RUNTIME_BIN}/openvino_ir_frontend.dll"
            "${OPENVINO_RUNTIME_BIN}/openvino_auto_plugin.dll"
            "${OPENVINO_RUNTIME_BIN}/openvino_hetero_plugin.dll"
            "${OPENVINO_TBB_BIN}/tbb12.dll"
            "${OPENVINO_TBB_BIN}/tbbmalloc.dll"
            "$<TARGET_FILE_DIR:${example_name}>"

        # 把 YOLO 模型和类别文件复制到 exe 目录，运行时用相对路径就能找到
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assents/yolov5s.onnx"
            "${CMAKE_CURRENT_SOURCE_DIR}/assents/coco.names"
            
            "${CMAKE_CURRENT_SOURCE_DIR}/assents_sp/yolov5.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/assents_sp/yolov5.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/assents_sp/yolov8.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/assents_sp/yolov8.bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/assents_sp/yolo11.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/assents_sp/yolo11.bin"

            "$<TARGET_FILE_DIR:${example_name}>"#这个不能删除
        COMMENT "Copying OpenCV DLLs and model files for ${example_name}"
    )
endforeach()
