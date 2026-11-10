// 相机驱动封装：包装 main 分支的 io::Camera（海康 USB3 Vision + USB/UVC），
// 并负责把标定文件转换为 ROS 的 CameraInfo。
#pragma once

#include <memory>
#include <string>

#include <opencv2/core.hpp>
#include <sensor_msgs/msg/camera_info.hpp>

namespace nr_vision_io
{
// 相机标定（像素单位 / OpenCV 畸变系数）
struct Calibration
{
  cv::Mat camera_matrix;  // 3x3, CV_64F
  cv::Mat dist_coeffs;    // 1xN, CV_64F
  int image_width{0};
  int image_height{0};

  bool valid() const { return !camera_matrix.empty() && camera_matrix.total() == 9; }
};

// 从 OpenCV FileStorage YAML 读取标定：
//   image_width / image_height / camera_matrix / distortion_coeffs
Calibration loadCalibration(const std::string & path);

class CameraDriver
{
public:
  CameraDriver(const std::string & camera_config_path, const std::string & calibration_path);
  ~CameraDriver();

  bool isOpened() const;
  bool read(cv::Mat & img);
  void close();

  const Calibration & calibration() const;

  // 用标定构造 CameraInfo；标定缺失时 K/D 为 0 并返回 false，调用方可据此降级
  bool fillCameraInfo(sensor_msgs::msg::CameraInfo & info, const std::string & frame_id) const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace nr_vision_io
