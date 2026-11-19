# nr-auto-aim ROS2 工作区（ros2_ws）

本目录是 auto-aim 的 **ROS2 运行时框架**：节点层只做订阅/发布/参数，算法与硬件驱动仍然复用 main 分支的代码，避免两份实现。

## 包结构与数据流

~~~text
ros2_ws/src/
├── rm_vision_interfaces/   # 消息：Armor / Armors / Target（沿用 ros2 分支）
├── nr_vision_io/           # 相机驱动：包装 main 的 io/（海康 MVS + USB/UVC）+ 标定/CameraInfo
├── nr_vision_detector/     # YOLO11 推理 + 38 类语义 + 4 关键点排序 + PnP（复用 ros2 分支 PnP 思路）
└── rm_vision_cpp/          # 节点：camera_node / armor_detector / armor_tracker
~~~

~~~text
[camera_node] --/image_raw + /camera_info--> [armor_detector] --/armors--> [armor_tracker] --/target-->
~~~

## 与 main 的关系（重要）

| 内容 | 归属 | 说明 |
|---|---|---|
| 相机驱动（MVS/UVC） | main 的 io/ | nr_vision_io 直接编译 `<repo>/io/*.cpp`，**不复制源码** |
| 相机配置 | main 的 configs/camera.yaml | camera_node 默认读它，可参数覆盖 |
| YOLO 模型 | main 的 assets/yolo11.xml|bin | detector 默认读它，可参数覆盖 |
| 38 类标签 | 本仓库 configs/armor_labels.txt + nr_vision_detector/class_labels.hpp | 与模型元数据 labels 顺序一致 |
| 标定 | 本仓库 configs/calibration.yaml | main 目前没有标定，ROS2 版先落地 |
| PnP / 关键点排序 / KF | nr_vision_detector、rm_vision_cpp | 从 ros2 分支移植并整理 |

## 依赖

~~~bash
# ROS2 Humble（Ubuntu 22.04）
sudo apt install -y ros-humble-desktop
sudo apt install -y libopencv-dev libeigen3-dev libyaml-cpp-dev   ros-humble-cv-bridge ros-humble-image-transport   ros-humble-tf2 ros-humble-tf2-ros ros-humble-tf2-geometry-msgs
# OpenVINO 2024.6 Runtime（按 docs/openVINOSDK.md 安装，或解压后 source setupvars.sh）
# 海康 MVS SDK 已随仓库提供：io/hikrobot（CMake 会自动使用，无需 LD_LIBRARY_PATH）
~~~

> OpenVINO 不在 rosdep 里，安装后确保 `find_package(OpenVINO)` 能找到（设置 `OpenVINO_DIR` 或 source setupvars）。

## 构建

~~~bash
cd <repo>/ros2_ws
colcon build --symlink-install
source install/setup.bash
~~~

## 运行

~~~bash
# 用海康/ USB 相机（configs/camera.yaml 选择）
ros2 launch rm_vision_cpp vision.launch.py

# 无硬件：用视频文件回放
ros2 launch rm_vision_cpp vision.launch.py video_file:=/path/to/test.mp4

# 只看某个话题
ros2 topic hz /image_raw
ros2 topic echo /armors
ros2 topic echo /target

# rviz2 / foxglove 看调试图（debug=true 时发布）
ros2 run rviz2 rviz2   # 添加 Image -> /armor_detector/debug_image
~~~

## 参数

- 节点参数：`rm_vision_cpp/config/params.yaml`（相机路径、模型路径、阈值、装甲尺寸、KF）
- 相机硬件参数：`configs/camera.yaml`（main）
- 标定：`configs/calibration.yaml`（先用棋盘格标定，否则 /camera_info 的 K 为 0，检测节点跳过位姿）

## 已知限制 / 下一步（按优先级）

1. **标定**：`configs/calibration.yaml` 目前是示例值，必须实测标定；
2. **TF/URDF**：tracker 支持 `target_frame:=odom`，但仓库还没有 URDF/robot_state_publisher，需补 `odom -> gimbal -> camera_optical_frame`；
3. **串口/CAN 节点**：尚未实现，`/target` 目前出不了本机（下一步接 `rm_serial_driver` 或自写节点）；
4. **rosbag2 回放 / gtest / CI / Docker**：尚未加入；
5. **类别集合**：38 类来自 2025 赛季模型（含 five/balance*/E/P），使用前按当季官方规则核对；
6. **零拷贝**：v1 用独立进程 + DDS（已用 SensorDataQoS），后续可把 camera+detector 放进 composable 容器开 intra-process。

## 常见问题

- `找不到海康 MVS SDK`：确认 `io/hikrobot/lib/amd64/libMvCameraControl.so` 与 `libMvUsb3vTL.so` 存在；arm64 平台需完整 arm64 库。
- `相机打开失败`：先跑 main 的 `camera_test` 验证同一份 `configs/camera.yaml`。
- `标定缺失`：`camera_info` 的 K 全 0 时不会发布 /armors，先补 `configs/calibration.yaml`。
- `OpenVINO 找不到`：`source <openvino>/setupvars.sh` 或设置 `OpenVINO_DIR`。
