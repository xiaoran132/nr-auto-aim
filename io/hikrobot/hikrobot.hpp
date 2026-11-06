// =====================================================================
// 海康机器人(HIKROBOT) USB3.0 机器视觉相机封装 —— 接口声明
//   型号: MV-CU013-AOUC (USB 厂商/产品 ID = 2bdf:0001)
//   实现见 hikrobot.cpp；依赖 io/hikrobot 下的 MVS SDK。
//   USB3 Vision 工业相机不是 UVC，需经 MVS SDK 取流。
// =====================================================================

#pragma once

#include <opencv2/opencv.hpp>
#include <string>

#include "io/camera.hpp"

class HikRobotCamera : public io::CameraBase {
public:
    HikRobotCamera();
    ~HikRobotCamera();

    // 枚举并打开第一台 USB 相机，设置曝光/增益后开始取流
    // exposure_ms: 曝光时间(毫秒)，自动瞄准通常用 1~3ms 短曝光
    // gain:        增益，默认 16
    bool open(double exposure_ms = 2.0, double gain = 16.0);

    // 从 YAML 配置读取相机参数后打开（读取 exposure_ms / gain）
    bool open(const std::string& config_path);

    // 取一帧，内部把 Bayer/Mono/RGB 统一转成 BGR 的 cv::Mat
    // 返回 false 表示取流失败(可能超时或掉线)
    bool read(cv::Mat& bgr) override;

    void close() override;

    bool isOpened() const;

private:
    void* handle_;   // MV_CC 句柄，用 void* 避免在头文件暴露 MVS 类型
    bool  grabbing_;
};
