# 从 origin/ros2 迁移到 ros2_ws

## 目录映射

| 旧（origin/ros2） | 新（ros2_ws） | 说明 |
|---|---|---|
| src/rm_vision_interfaces/ | ros2_ws/src/rm_vision_interfaces/ | 原样保留（消息字段不变） |
| src/rm_vision_cpp/src/video_source_node.cpp | 由 rm_vision_cpp/src/camera_node.cpp 取代 | 改为使用 main 的 io::Camera（海康 MVS + UVC），支持 video_file 回放 |
| src/rm_vision_cpp/src/armor_detector_node.cpp | rm_vision_cpp/src/detector_node.cpp | 使用 nr_vision_detector（YOLO11），不再用传统灯条法 |
| src/rm_vision_cpp/src/armor_detect.cpp（传统检测） | nr_vision_detector/src/armor_pose.cpp（只保留 PnP） | 传统灯条检测未迁移（可后续作为 backend 补回） |
| src/rm_vision_cpp/src/armor_tracker_node.cpp | rm_vision_cpp/src/tracker_node.cpp | 改为用 header.stamp 算 dt，可选 tf2 变换到 target_frame |
| src/rm_vision_cpp/src/kalman.cpp | rm_vision_cpp/src/kalman.cpp | 原样保留 |
| src/rm_vision_cpp/config/params.yaml | rm_vision_cpp/config/params.yaml | 重新分组：/camera_node /armor_detector /armor_tracker |
| src/rm_vision_cpp/launch/vision.launch.py | rm_vision_cpp/launch/vision.launch.py | 增加 camera_config / calibration_file / model / video_file / enemy_color 参数 |
| （无） | nr_vision_io/ | 新增：包装 main 的 io/ + 标定 + CameraInfo |
| （无） | nr_vision_detector/ | 新增：YOLO11 + 38 类语义 + 关键点排序 + PnP |

## 与 main 对齐的点

1. **相机只有一份实现**：nr_vision_io 直接编译 `<repo>/io/camera.cpp`、`io/usbcamera/*`、`io/hikrobot/*`；
2. **配置只有一份**：相机用 `configs/camera.yaml`，模型用 `assets/yolo11.xml|bin`，不另拷；
3. **类别语义**：`configs/armor_labels.txt` 与 `nr_vision_detector/class_labels.hpp` 都是模型 labels 的顺序，替换了旧的 `class0..class37` 占位；
4. **装甲尺寸**：小 135×55 mm / 大 225×55 mm 改为参数（与 ros2 分支 PnP 一致），可调；
5. **构建边界**：ROS2 用 colcon 只扫 `ros2_ws/src`，main 的 CMake demo 不受影响。

## 如何在仓库上应用

~~~bash
git checkout main
git checkout -b ros2-integration
git merge --no-ff origin/ros2        # 实测无冲突（只新增 17 个文件）
git rm -r src/rm_vision_cpp src/rm_vision_interfaces   # 旧原型，已被 ros2_ws 取代
git checkout <本分支> -- ros2_ws configs/armor_labels.txt configs/calibration.yaml .gitignore
git commit -m "feat(ros2): 重构为 ros2_ws 四包结构，接入 main 的相机/YOLO/PnP"
~~~

## 验收清单

- [ ] `cd ros2_ws && colcon build --symlink-install` 通过（4 个包）
- [ ] `ros2 launch rm_vision_cpp vision.launch.py video_file:=...` 能出 /armors
- [ ] 接相机后 `ros2 topic hz /image_raw` ≥ 30
- [ ] 标定后 `/armors` 的距离在 3 m 处误差 ≤ 5%
- [ ] `/target` 丢帧后 0.5 s 内保持外推
- [ ] 可选：`target_frame:=odom` + URDF 后 /target 在惯性系下
