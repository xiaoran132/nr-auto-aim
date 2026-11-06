add_library(io STATIC
    camera.cpp
    hikrobot/hikrobot.cpp
    usbcamera/usbcamera.cpp)

target_include_directories(io PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/hikrobot/include)

# MVS SDK：按架构选择 lib/amd64 或 lib/arm64
target_link_directories(io PUBLIC ${NR_HIKROBOT_LIB_DIR})

target_link_libraries(io PUBLIC
    ${OpenCV_LIBS}
    tools
    MvCameraControl)
