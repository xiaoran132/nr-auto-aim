#pragma once

#include <memory>
#include <opencv2/opencv.hpp>
#include <string>

namespace io
{
// 所有相机的统一接口：调用方只依赖 read()/close()
class CameraBase
{
public:
  virtual ~CameraBase() = default;
  virtual bool read(cv::Mat & img) = 0;
  virtual void close() = 0;

  // 通用参数读写：prop 使用 cv::CAP_PROP_* 常量
  // 不支持的相机返回 false / -1
  virtual bool setProperty(int prop, double value)
  {
    (void)prop;
    (void)value;
    return false;
  }
  virtual double getProperty(int prop) const
  {
    (void)prop;
    return -1.0;
  }
};

// 相机工厂：根据 configs/camera.yaml 的 camera_name 创建对应实现
class Camera
{
public:
  explicit Camera(const std::string & config_path);

  bool read(cv::Mat & img);
  void close();
  bool isOpened() const;

  bool setProperty(int prop, double value);
  double getProperty(int prop) const;

private:
  std::unique_ptr<CameraBase> camera_;
};

}  // namespace io
