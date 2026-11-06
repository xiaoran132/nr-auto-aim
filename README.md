# nr-auto-aim

重庆移通学院 NR 战队的自动瞄准算法。

## 目录结构

```text
nr-auto-aim/
├── assets/              # 模型、类别文件、示例视频
│   └── demo/
├── configs/             # 运行参数（camera.yaml 等）
├── docs/                # 环境与接入文档
├── io/                  # 硬件 I/O 模块
│   └── hikrobot/        # 海康 MVS SDK + 相机封装
├── src/                 # 可执行程序
├── tests/               # 测试程序
├── tools/               # 公共工具（yaml 读取等）
├── CMakeLists.txt
└── CMakePresets.json
```

## 依赖

```bash
sudo apt install libopencv-dev libopenvino-dev libyaml-cpp-dev
```

## 构建

```bash
cmake --preset "GCC 15.2.0 x86_64-linux-gnu"
cmake --build "out/build/GCC 15.2.0 x86_64-linux-gnu" -j"$(nproc)"
```

也可以直接配置：

```bash
cmake -S . -B out/build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/debug -j"$(nproc)"
```

所有可执行文件都通过 `PROJECT_ROOT_DIR` 定位 `assets/` 与 `configs/`，
因此可以在任意工作目录启动。

## 常用程序

| 程序 | 说明 |
|------|------|
| `04_check_model` | 打印 ONNX 模型输出形状 |
| `camera_test` | 相机取流测试（无界面） |
| `camera_viewer` | 相机实时窗口 + 参数滑条，`s`/空格抓拍，`d` 导出 YAML，`q`/ESC 退出 |
| `camera_yolo` | 相机 + YOLO11 实时识别 |
| `opcv_yolo5/8/11` | 各版本 YOLO 推理示例 |
| `video_yolo` | 视频文件 + YOLO11 识别 |
| `yolo_test` | OpenCV DNN + YOLOv5 ONNX 入门示例 |

相机由 `configs/camera.yaml` 的 `camera_name` 选择：

- `usbcamera`：普通 USB / UVC 摄像头（`cv::VideoCapture`）
- `hikrobot`：海康 USB3 Vision 工业相机（MVS SDK）

两类相机各自有独立的工具类（`UsbCamera` / `HikRobotCamera`），
通过 `io::Camera` 工厂统一创建，业务代码只调用 `read()` / `close()`。

USB / UVC 摄像头建议使用 `fourcc: "MJPG"`（YUYV 在部分摄像头上会花屏）；
若自动曝光不可用，可设置 `auto_exposure: 0` 并手动指定 `exposure` / `brightness`。

## 海康相机

安装 udev 规则后重新插拔相机：

```bash
sudo cp io/hikrobot/99-hikrobot.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

详细说明见 `docs/海康相机MVS接入指南.md`。
