#include "usbcamera.hpp"

#include <cstdio>

#include "tools/yaml.hpp"

namespace io
{
bool UsbCamera::open(const std::string & config_path)
{
  try {
    const YAML::Node yaml = tools::load(config_path);

    const int device_index = tools::read<int>(yaml, "device_index");
    const std::string fourcc = tools::read<std::string>(yaml, "fourcc");
    const int width        = tools::read<int>(yaml, "image_width");
    const int height       = tools::read<int>(yaml, "image_height");
    const int fps          = tools::read<int>(yaml, "fps");

    if (fourcc.size() != 4) {
      std::fprintf(stderr, "[UsbCamera] fourcc 必须是 4 个字符（如 MJPG / YUYV）\n");
      return false;
    }

    // 优先用 V4L2 后端；失败时退回 OpenCV 默认后端
    cap_.open(device_index, cv::CAP_V4L2);
    if (!cap_.isOpened()) cap_.open(device_index);

    if (!cap_.isOpened()) {
      std::fprintf(stderr, "[UsbCamera] 无法打开 /dev/video%d\n", device_index);
      return false;
    }

    // 先设像素格式，再设分辨率/帧率：部分 UVC 摄像头在 YUYV 高带宽格式下
    // 会丢包花屏，强制 MJPG 可稳定取流。
    cap_.set(
      cv::CAP_PROP_FOURCC,
      cv::VideoWriter::fourcc(fourcc[0], fourcc[1], fourcc[2], fourcc[3]));
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    cap_.set(cv::CAP_PROP_FPS, fps);

    // 可选图像控制。部分摄像头自动曝光不可靠，可改成手动：
    //   auto_exposure: 0（手动）, 1（自动）
    auto set_opt = [&](const char * name, int prop, const YAML::Node & node) {
      if (!node) return;
      const int value = node.as<int>();
      if (!cap_.set(prop, value))
        std::fprintf(
          stderr, "[UsbCamera] %s=%d 设置失败（相机不支持或当前模式不允许）\n", name,
          value);
    };
    set_opt("auto_exposure", cv::CAP_PROP_AUTO_EXPOSURE, yaml["auto_exposure"]);
    set_opt("exposure", cv::CAP_PROP_EXPOSURE, yaml["exposure"]);
    set_opt("gain", cv::CAP_PROP_GAIN, yaml["gain"]);
    set_opt("brightness", cv::CAP_PROP_BRIGHTNESS, yaml["brightness"]);
    set_opt("contrast", cv::CAP_PROP_CONTRAST, yaml["contrast"]);
    set_opt("saturation", cv::CAP_PROP_SATURATION, yaml["saturation"]);
    set_opt("hue", cv::CAP_PROP_HUE, yaml["hue"]);
    set_opt("sharpness", cv::CAP_PROP_SHARPNESS, yaml["sharpness"]);
    set_opt("gamma", cv::CAP_PROP_GAMMA, yaml["gamma"]);
    set_opt("auto_wb", cv::CAP_PROP_AUTO_WB, yaml["auto_wb"]);

    std::fprintf(
      stderr, "[UsbCamera] /dev/video%d 已打开: %dx%d @ %.0f fps, fourcc=%s\n",
      device_index, static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH)),
      static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT)), cap_.get(cv::CAP_PROP_FPS),
      fourcc.c_str());
    return true;
  } catch (const std::exception & e) {
    std::fprintf(stderr, "[UsbCamera] 读取配置失败: %s\n", e.what());
    return false;
  }
}

bool UsbCamera::read(cv::Mat & img)
{
  if (!cap_.isOpened()) return false;
  cap_ >> img;
  return !img.empty();
}

void UsbCamera::close()
{
  if (cap_.isOpened()) cap_.release();
}

bool UsbCamera::isOpened() const { return cap_.isOpened(); }

bool UsbCamera::setProperty(int prop, double value)
{
  return cap_.isOpened() && cap_.set(prop, value);
}

double UsbCamera::getProperty(int prop) const
{
  return cap_.isOpened() ? cap_.get(prop) : -1.0;
}

}  // namespace io
