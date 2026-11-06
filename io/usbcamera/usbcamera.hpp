#pragma once

#include <opencv2/opencv.hpp>
#include <string>

#include "io/camera.hpp"

namespace io
{
// 普通 USB / UVC 摄像头（如笔记本摄像头、USB 网络摄像头）
//   走 V4L2 + cv::VideoCapture，不需要任何厂商 SDK
class UsbCamera : public CameraBase
{
public:
  UsbCamera() = default;
  ~UsbCamera() override { close(); }

  bool open(const std::string & config_path);
  bool read(cv::Mat & img) override;
  void close() override;
  bool isOpened() const;

  bool setProperty(int prop, double value) override;
  double getProperty(int prop) const override;

private:
  cv::VideoCapture cap_;
};

}  // namespace io
