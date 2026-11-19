#include "nr_vision_io/camera_driver.hpp"

#include <cstdio>
#include <memory>

#include <opencv2/core.hpp>

#include "io/camera.hpp"

namespace nr_vision_io
{
namespace
{
void readMat(cv::FileStorage & fs, const char * key, cv::Mat & out)
{
  const cv::FileNode node = fs[key];
  if (!node.empty()) {
    node >> out;
  }
}
}  // namespace

Calibration loadCalibration(const std::string & path)
{
  Calibration calib;
  cv::FileStorage fs;
  try {
    fs.open(path, cv::FileStorage::READ);
  } catch (const cv::Exception &) {
    fs.release();
  }
  if (!fs.isOpened()) {
    std::fprintf(
      stderr,
      "[nr_vision_io] 未找到标定文件: %s\n"
      "              先用棋盘格 + cv::calibrateCamera 生成，否则无法解算位姿\n",
      path.c_str());
    return calib;
  }

  fs["image_width"] >> calib.image_width;
  fs["image_height"] >> calib.image_height;
  readMat(fs, "camera_matrix", calib.camera_matrix);
  readMat(fs, "distortion_coeffs", calib.dist_coeffs);

  if (!calib.camera_matrix.empty()) {
    calib.camera_matrix.convertTo(calib.camera_matrix, CV_64F);
  }
  if (calib.dist_coeffs.empty()) {
    calib.dist_coeffs = cv::Mat::zeros(1, 5, CV_64F);
  } else {
    calib.dist_coeffs.convertTo(calib.dist_coeffs, CV_64F);
  }
  if (!calib.valid()) {
    std::fprintf(stderr, "[nr_vision_io] 标定文件 %s 格式不完整\n", path.c_str());
    calib = Calibration{};
  }
  return calib;
}

class CameraDriver::Impl
{
public:
  Impl(const std::string & camera_config_path, const std::string & calibration_path)
  : camera_(camera_config_path), calib_(loadCalibration(calibration_path))
  {
  }

  io::Camera camera_;
  Calibration calib_;
};

CameraDriver::CameraDriver(
  const std::string & camera_config_path, const std::string & calibration_path)
: impl_(std::make_unique<Impl>(camera_config_path, calibration_path))
{
}

CameraDriver::~CameraDriver()
{
  if (impl_) {
    impl_->camera_.close();
  }
}

bool CameraDriver::isOpened() const { return impl_ && impl_->camera_.isOpened(); }

bool CameraDriver::read(cv::Mat & img) { return impl_ && impl_->camera_.read(img); }

void CameraDriver::close()
{
  if (impl_) {
    impl_->camera_.close();
  }
}

const Calibration & CameraDriver::calibration() const
{
  static const Calibration empty;
  return impl_ ? impl_->calib_ : empty;
}

bool CameraDriver::fillCameraInfo(
  sensor_msgs::msg::CameraInfo & info, const std::string & frame_id) const
{
  info.header.frame_id = frame_id;
  info.distortion_model = "plumb_bob";

  const Calibration & calib = calibration();
  if (!calib.valid()) {
    info.d = {0.0, 0.0, 0.0, 0.0, 0.0};
    info.k.fill(0.0);
    info.r = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    info.p.fill(0.0);
    return false;
  }

  info.width = static_cast<uint32_t>(calib.image_width);
  info.height = static_cast<uint32_t>(calib.image_height);
  info.d.resize(calib.dist_coeffs.total());
  for (size_t i = 0; i < calib.dist_coeffs.total(); ++i) {
    info.d[i] = calib.dist_coeffs.at<double>(static_cast<int>(i));
  }
  const double * k = calib.camera_matrix.ptr<double>();
  for (int i = 0; i < 9; ++i) {
    info.k[i] = k[i];
  }
  info.r = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
  info.p = {
    info.k[0], info.k[1], info.k[2], 0.0,
    info.k[3], info.k[4], info.k[5], 0.0,
    info.k[6], info.k[7], info.k[8], 0.0};
  return true;
}

}  // namespace nr_vision_io
