#openVINOruntime SDK 2024.6下载与安装
这里提供两个下载地址
手动下载，压缩包
地址为
https://storage.openvinotoolkit.org/repositories/openvino/packages/2024.6/windows/w_openvino_toolkit_windows_2024.6.0.17404.4c0f47d2335_x86_64.zip

附带SHA256校验文件
https://storage.openvinotoolkit.org/repositories/openvino/packages/2024.6/windows/w_openvino_toolkit_windows_2024.6.0.17404.4c0f47d2335_x86_64.zip.sha256

说明:这是2024.6的release版(build 17404,hash 4c0f47d2335),不是.dev预发布版。同一目录下还有 2024.6.0.dev202412xx 之类的dev版本,别选错。


或者采用命令行下载方式（powershell）
curl.exe -L -o openvino_2024.6.zip "https://storage.openvinotoolkit.org/repositories/openvino/packages/2024.6/windows/w_openvino_toolkit_windows_2024.6.0.17404.4c0f47d2335_x86_64.zip"

校验
CertUtil -hashfile openvino_2024.6.zip SHA256

手动安装步骤，先解压ZIP到任意目录。
w_openvino_toolkit_windows_2024.6.0.17404.4c0f47d2335_x86_64\
├── runtime\
│   ├── bin\intel64\Release\        ← 运行时 DLL（openvino.dll、*_plugin.dll、*_frontend.dll）
│   ├── lib\intel64\Release\        ← 链接库（openvino.lib 等）————————————>>>>AAA
│   ├── include\                    ← C++ 头文件
│   ├── cmake\OpenVINOConfig.cmake  ← CMake 查找用（关键）
│   └── 3rdparty\tbb\bin\           ← TBB 依赖（tbb12.dll、tbbmalloc.dll）--------->>>>BBB
├── setupvars.bat                   ← 一键设置 PATH 等环境变量
├── setupvars.ps1
├── samples\ / python\ / docs\

然后设置环境变量了，
○ 运行
○ 或手动把 runtime\bin\inte164\Release 加入 PATH(环境变量)

setupvars. bat

(临时,只对当前命令行窗口生效);
(供openvino. d11及插件被加载)。


然后手动修改，CmakeLists文件当中的
set(OPENVINO_RUNTIME_BIN "C:/dev/openvino_sdk/w_openvino_toolkit_windows_2024.6.0.17404.4c0f47d2335_x86_64/runtime/bin/intel64/Release")AAA
set(OPENVINO_TBB_BIN     "C:/dev/openvino_sdk/w_openvino_toolkit_windows_2024.6.0.17404.4c0f47d2335_x86_64/runtime/3rdparty/tbb/bin")BBB

if(NOT OpenCV_DIR AND DEFINED ENV{OpenCV_DIR})
    set(OpenCV_DIR "$ENV{OpenCV_DIR}")
endif()
if(NOT OpenCV_DIR)
    set(OpenCV_DIR "C:/dev/opencv/install_ov/lib")——————————>>>DDD
endif()
find_package(OpenCV REQUIRED)



将这个AAA的地址换成你实际解压后的位置，
将这个BBB的地址换成你实际解压后的位置，

将这个DDD地址换成自己编译/安装的带 OpenVINO的OpenCV的1ib 目录
