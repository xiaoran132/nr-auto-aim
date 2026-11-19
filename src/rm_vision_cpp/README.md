# rm_vision_cpp

RoboMaster 视觉系统的 C++ 实现，工程结构参考 `chenjunnn/rm_vision`：

```
视频源 -> /image_raw -> 检测 -> /armors -> 跟踪 -> /target
```

## 依赖

```bash
sudo apt install -y \
  ros-humble-cv-bridge \
  libopencv-dev \
  libeigen3-dev
```

## 编译

```bash
cd ~/ros_ws
colcon build --symlink-install
source install/setup.bash
```

## 运行

```bash
# 摄像头
ros2 launch rm_vision_cpp vision.launch.py source:=0

# 视频文件
ros2 launch rm_vision_cpp vision.launch.py source:=/home/yyy/test.mp4

# 图片目录
ros2 launch rm_vision_cpp vision.launch.py source:=/home/yyy/images
```

## 查看

```bash
ros2 topic list
ros2 topic echo /armors
ros2 topic echo /target
ros2 run rqt_image_view rqt_image_view      # 选 /armor_detector/debug_image
```

## 目录

```
include/rm_vision_cpp/
├── armor_detect.hpp      纯算法：灯条法 + PnP
└── kalman.hpp            匀速模型卡尔曼
src/
├── armor_detect.cpp
├── kalman.cpp
├── video_source_node.cpp
├── armor_detector_node.cpp
└── armor_tracker_node.cpp
```

**分层原则**：`armor_detect.cpp` 与 `kalman.cpp` 不依赖 ROS，
可以在任何 C++ 程序里单独使用；ROS 节点只是薄薄一层壳。

## 单位约定

| 字段 | 单位 |
| --- | --- |
| `Armor.pose.position` | 米 |
| `Armor.distance` | 毫米 |
| `Armor.yaw` / `Armor.pitch` | 度 |
| `Target.position` / `velocity` | 米 / 米每秒 |
| `Target.yaw` / `v_yaw` | 弧度 |
