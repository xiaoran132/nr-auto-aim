# 海康工业相机（HIKROBOT MV-CU013）MVS 接入指南

本文档说明如何在本项目（`nr-auto-aim`）中接入海康机器人 USB3.0 工业相机，涵盖 **MVS SDK 的配置**、**代码集成方式**，以及**常见问题排查**。

---

## 1. 概述

| 项目 | 说明 |
|------|------|
| 相机型号 | 海康机器人 HIKROBOT **MV-CU013-AOUC**（USB3.0，1.3MP，1280×1024） |
| 厂商/产品 ID | `2bdf:0001` |
| 协议 | **USB3 Vision**（不是 UVC，也不是网络 RTSP） |
| 取流方式 | 必须用海康 **MVS 机器视觉 SDK**（`libMvCameraControl.so` + `libMvUsb3vTL.so`） |
| SDK 版本 | MVS 5.1.1（SDK 核心 V4.9.0） |
| 实测帧率 | ~96 fps（1280×1024，Bayer→BGR） |

> ⚠️ 三个容易混淆的概念：
> - **UVC 摄像头**：Linux 内核自动生成 `/dev/video*`，OpenCV `VideoCapture(0)` 直接读，不需要 SDK。海康的 DS-E12/E14 属此类。
> - **网络摄像头**：走 RTSP，用 `rtsp://user:pass@ip:554/...` 拉流。
> - **本机这种工业相机（MV-CU 系列）**：走 USB3 Vision，**必须用 MVS SDK**，不会生成 `/dev/video*`。

---

## 2. SDK 文件位置

完整 MVS SDK 已拷贝进项目，位于 `io/hikrobot/`：

```
io/hikrobot/
├── include/                        # SDK 头文件（7 个 .h）
│   ├── MvCameraControl.h           # 核心 API（枚举/打开/取流/参数设置）
│   ├── CameraParams.h
│   ├── MvErrorDefine.h             # 错误码定义
│   ├── PixelType.h                 # 像素格式枚举（Bayer8/BGR8 等）
│   └── ...
├── lib/
│   ├── amd64/                      # ★ x86_64 完整运行库（本机使用）
│   │   ├── libMvCameraControl.so   # 核心控制库（符号链接 → .so.4.9.0.3）
│   │   ├── libMvUsb3vTL.so         # ★ USB3 Vision 传输层（缺它枚举不到相机）
│   │   ├── liblog4cpp_gcc485_v3_0.so
│   │   ├── libGCBase_gcc485_v3_0.so
│   │   ├── libGenCP_gcc485_v3_0.so
│   │   ├── MvProducerU3V.cti       # USB3 Vision 的 GenICam 生产者
│   │   ├── CommonParameters.ini    # 参数默认值
│   │   ├── libusb-1.0.so.0
│   │   ├── ThirdParty/             # libavutil / libswscale
│   │   └── ...（共 36 个文件）
│   └── arm64/                      # 遗留的残缺旧版，仅 libMvCameraControl.so，勿用
└── 99-hikrobot.rules               # udev 授权规则（见 3.2）
```

---

## 3. MVS 配置

### 3.1 为什么必须用「完整」SDK（血泪坑）

`libMvCameraControl.so` 运行时会用 `dlopen` 动态加载 **`libMvUsb3vTL.so`**（USB3 Vision 传输层）和 `liblog4cpp_gcc485_v3_0.so`（日志）。

如果只拷贝 `libMvCameraControl.so` 一个文件（例如从队友旧项目 `sp_vision_25/io/hikrobot/lib` 里拷），缺少 `libMvUsb3vTL.so`，就会：

```
[HikRobot] 未找到 USB 相机 (MV_CC_EnumDevices=0x80000006, 设备数=0)
```

错误码 `0x80000006` = 「资源申请失败」。**根治办法：用海康官网的完整 MVS Linux x86_64 安装包，把 `lib/64/` 目录整个拷过来**（官网：https://www.hikrobotics.com ，需注册登录，下载「MVS 机器视觉开发套件」）。

### 3.2 USB 权限（udev 规则）

工业相机取流需要普通用户对 USB 设备有**写权限**（做控制传输）。默认设备节点是 `root:root 0664`，普通用户只读，会导致同样报 `0x80000006`。

项目里已提供规则文件 `io/hikrobot/99-hikrobot.rules`，内容：

```
# 海康威视/海康机器人 厂商 ID = 2bdf
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTR{idVendor}=="2bdf", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="2bdf", MODE="0666"
```

安装（需要 sudo，装一次永久生效）：

```bash
sudo cp io/hikrobot/99-hikrobot.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

装完**重新插拔一次相机**（或 VMware 里重新连接该 USB 设备）。

### 3.3 运行库查找（RPATH）

CMake 已通过 `BUILD_RPATH` 把 `io/hikrobot/lib/amd64` 写进每个可执行文件的运行时搜索路径，所以运行程序时**不用手动设 `LD_LIBRARY_PATH`**。

---

## 4. 代码集成方式

### 4.1 相机封装接口

相机封装拆成两个文件：

- `io/hikrobot/hikrobot.hpp` —— 接口声明
- `io/hikrobot/hikrobot.cpp` —— 实现（内部调用 MVS SDK）

对外只有一个类 `HikRobotCamera`：

```cpp
class HikRobotCamera {
public:
    HikRobotCamera();
    ~HikRobotCamera();

    // 枚举并打开第一台 USB 相机，设置曝光/增益后开始取流
    bool open(double exposure_ms = 2.0, double gain = 16.0);

    // 从 YAML 配置读取相机参数后打开（读取 exposure_ms / gain）
    bool open(const std::string& config_path);

    // 取一帧，内部把 Bayer/Mono/RGB 统一转成 BGR 的 cv::Mat
    bool read(cv::Mat& bgr);

    void close();
    bool isOpened() const;
};
```

**任何识别程序接入相机的最小套路：**

```cpp
#include "io/hikrobot/hikrobot.hpp"     // ① 引入头文件

HikRobotCamera cam;                     // ② 声明相机对象
cam.open(PROJECT_ROOT_DIR "/configs/camera.yaml");  // ③ 读配置打开

cv::Mat frame;
while (cam.read(frame)) {               // ④ 循环取帧，frame 是 BGR 的 cv::Mat
    // ... 你的 YOLO / 传统视觉处理 ...
}
cam.close();                            // ⑤ 关闭
```

### 4.2 在 YOLO 识别文件里接入（示例：`src/opcv_yolo11.cpp`）

把原来的 `cv::VideoCapture cap(0)`（UVC 摄像头）换成海康相机，共 4 处改动：

```cpp
// ① 顶部加头文件
#include "io/hikrobot/hikrobot.hpp"

// ② 打开相机（原来 cv::VideoCapture cap(0);）
HikRobotCamera cam;
const bool opened = (argc > 2)
    ? cam.open(atof(argv[1]), atof(argv[2]))              // 命令行优先
    : cam.open(PROJECT_ROOT_DIR "/configs/camera.yaml");  // 否则读配置
if (!opened) { return -1; }

// ③ 取帧（原来 cap >> frame; if(frame.empty())）
if (!cam.read(frame)) { break; }

// ④ 收尾（原来 cap.release();）
cam.close();
```

其余推理代码（letterbox / blob / infer / 后处理 / 画框）完全不用动。

### 4.3 在 USB 摄像头与海康相机之间切换

两类相机使用不同的实现类：

| `camera_name` | 实现类 | 取流方式 |
|---|---|---|
| `usbcamera` | `io::UsbCamera` | OpenCV `cv::VideoCapture` + V4L2 |
| `hikrobot` | `HikRobotCamera` | 海康 MVS SDK |

`io::Camera` 工厂读取 `configs/camera.yaml` 的 `camera_name`，创建对应实现，
所以 `camera_test`、`camera_yolo`、`opcv_yolo11` 不需要改代码即可切换：

```yaml
camera_name: "usbcamera"
device_index: 0
fourcc: "MJPG"       # 部分摄像头用 YUYV 会丢包花屏
image_width: 640
image_height: 480
fps: 30
auto_exposure: 0     # 0 = 手动，1 = 自动
exposure: 100
brightness: 128
```

改回海康：

```yaml
camera_name: "hikrobot"
exposure_ms: 2
gain: 16
vid_pid: "2bdf:0001"
```

### 4.4 CMake 配置

相机代码已归入 `io` 模块，`io/CMakeLists.txt` 把它编成静态库：

```cmake
add_library(io STATIC hikrobot/hikrobot.cpp)

target_include_directories(io PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/hikrobot/include)

target_link_directories(io PUBLIC ${NR_HIKROBOT_LIB_DIR})
target_link_libraries(io PUBLIC ${OpenCV_LIBS} tools MvCameraControl)
```

根 `CMakeLists.txt` 按架构设置 `NR_HIKROBOT_LIB_DIR`（x86_64 → `io/hikrobot/lib/amd64`），
需要相机的可执行文件只写 `target_link_libraries(<target> PRIVATE io ...)`，
并通过 `BUILD_RPATH` 在运行时定位 `libMvCameraControl.so`。

---

## 5. 编译与运行

```bash
# 配置（cmake 4.x 需加 CMAKE_POLICY_VERSION_MINIMUM）
CMAKE_POLICY_VERSION_MINIMUM=3.5 cmake --preset "GCC 15.2.0 x86_64-linux-gnu"

# 编译
cmake --build "out/build/GCC 15.2.0 x86_64-linux-gnu" -j$(nproc)

# 无界面取流测试（抓 30 帧，打印帧率，存一张图）
./out/build/GCC\ 15.2.0\ x86_64-linux-gnu/camera_test 30

# 相机 + YOLO11 完整识别（ESC 退出）
./out/build/GCC\ 15.2.0\ x86_64-linux-gnu/camera_yolo
./out/build/GCC\ 15.2.0\ x86_64-linux-gnu/opcv_yolo11   # 接入相机的 opcv_yolo11 版本

# 手动指定曝光/增益
./out/build/GCC\ 15.2.0\ x86_64-linux-gnu/opcv_yolo11 2.0 16
```

---

## 6. 相机参数调整

| 参数 | 当前值 | 说明 |
|------|--------|------|
| 曝光时间 | 2 ms | 自动瞄准用短曝光减少运动模糊，单位 us 传给 SDK |
| 增益 | 16 | 曝光不足时可适当加大 |
| 分辨率 | 1280×1024 | 相机原生 |
| 帧率 | ~96 fps | 当前未显式设帧率，跑在相机默认 |

在 `io/hikrobot/hikrobot.cpp` 的 `open()` 里已做：

```cpp
MV_CC_SetEnumValue(handle_, "BalanceWhiteAuto", MV_BALANCEWHITE_AUTO_CONTINUOUS); // 自动白平衡开
MV_CC_SetEnumValue(handle_, "ExposureAuto", MV_EXPOSURE_AUTO_MODE_OFF);            // 自动曝光关
MV_CC_SetEnumValue(handle_, "GainAuto", MV_GAIN_MODE_OFF);                         // 自动增益关
MV_CC_SetFloatValue(handle_, "ExposureTime", exposure_ms * 1000.0);                // 手动曝光(us)
MV_CC_SetFloatValue(handle_, "Gain", gain);                                        // 手动增益
```

如需固定帧率，可在 `StartGrabbing` 前加 `MV_CC_SetFrameRate(handle_, 150);`。

---

## 7. 常见问题排查

### 7.1 报 `0x80000006`（资源申请失败）且设备数=0

两种原因，按顺序排查：

1. **SDK 不完整**（缺 `libMvUsb3vTL.so`）——检查 `io/hikrobot/lib/amd64/` 里是否有 `libMvUsb3vTL.so`。没有就按 3.1 节补完整包。
2. **USB 权限不足**——检查 `ls -la /dev/bus/usb/*/`，海康设备节点是否为 `0666`（`crw-rw-rw-`）。不是就按 3.2 节装 udev 规则。

### 7.2 相机根本没出现在虚拟机里

```bash
lsusb | grep 2bdf    # 应能看到 Hikrobot MV-CU013
```

看不到说明 USB 设备还没透传给虚拟机。VMware 里：**虚拟机 → 可移动设备 → 连接该相机**（或点右下角 USB 图标）。

### 7.3 怎么确认是「透传问题」还是「SDK 问题」

用 libusb 直接测试（能 open + claim 接口 = 透传正常，问题在 SDK）：

```c
libusb_open(handle)           // 成功 = 透传 OK
libusb_claim_interface(h, 0)  // 成功 = 控制传输 OK
```

本项目已验证：VMware 透传海康 MV-CU013 完全正常（USB3 Vision 走 Bulk 传输，非等时传输）。

### 7.4 画面偏色（绿/紫）

说明 Bayer 模式判断错了。`io/hikrobot/hikrobot.cpp` 里已按 `raw.stFrameInfo.enPixelType` 自动选 `COLOR_BayerGR2BGR` / `RG` / `GB` / `BG`，一般不会错；若仍偏色，打印 `enPixelType` 核对是哪种 Bayer 排列。

---

## 附：关键文件清单

| 文件 | 作用 |
|------|------|
| `io/hikrobot/` | 完整 MVS SDK + udev 规则 |
| `io/hikrobot/hikrobot.hpp/.cpp` | 相机封装（接口 + 实现） |
| `tests/camera_test.cpp` | 无界面取流测试（相机类型由配置决定） |
| `src/camera_yolo.cpp` | 相机 + YOLO11 完整示例 |
| `src/opcv_yolo11.cpp` | 接入相机的 opcv_yolo11 示例 |
| `CMakeLists.txt` | 静态库 + 链接 + RPATH 配置 |
