#include "camera.hpp"

#include <cstdio>

#include "hikrobot/hikrobot.hpp"
#include "tools/yaml.hpp"
#include "usbcamera/usbcamera.hpp"

namespace io
{
Camera::Camera(const std::string & config_path)
{
  try {
    const YAML::Node yaml = tools::load(config_path);
    const std::string name = tools::read<std::string>(yaml, "camera_name");

    if (name == "hikrobot") {
      auto cam = std::make_unique<HikRobotCamera>();
      if (!cam->open(config_path)) return;
      camera_ = std::move(cam);
    }

    else if (name == "usbcamera") {
      auto cam = std::make_unique<UsbCamera>();
      if (!cam->open(config_path)) return;
      camera_ = std::move(cam);
    }

    else {
      std::fprintf(stderr, "[Camera] 未知 camera_name: \"%s\"\n", name.c_str());
    }
  } catch (const std::exception & e) {
    std::fprintf(stderr, "[Camera] 读取配置失败: %s\n", e.what());
  }
}

bool Camera::read(cv::Mat & img) { return camera_ && camera_->read(img); }

void Camera::close()
{
  if (camera_) camera_->close();
}

bool Camera::isOpened() const { return camera_ != nullptr; }

bool Camera::setProperty(int prop, double value)
{
  return camera_ && camera_->setProperty(prop, value);
}

double Camera::getProperty(int prop) const
{
  return camera_ ? camera_->getProperty(prop) : -1.0;
}

}  // namespace io
