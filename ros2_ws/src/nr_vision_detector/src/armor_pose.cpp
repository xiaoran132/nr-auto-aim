#include "nr_vision_detector/armor_pose.hpp"

#include <algorithm>
#include <cmath>

#include <opencv2/calib3d.hpp>

namespace nr_vision_detector
{
namespace
{
constexpr double kDegPerRad = 57.29577951308232;
}  // namespace

std::array<cv::Point3f, 4> objectPoints(double width_mm, double height_mm)
{
  const double half_w = width_mm * 0.5;
  const double half_h = height_mm * 0.5;
  return {
    cv::Point3f(-half_w, -half_h, 0.0F),
    cv::Point3f(half_w, -half_h, 0.0F),
    cv::Point3f(half_w, half_h, 0.0F),
    cv::Point3f(-half_w, half_h, 0.0F)};
}

std::array<cv::Point2f, 4> sortCorners(const std::array<cv::Point2f, 4> & corners)
{
  std::array<cv::Point2f, 4> pts = corners;
  std::sort(pts.begin(), pts.end(), [](const cv::Point2f & a, const cv::Point2f & b) {
    return a.y < b.y;
  });

  std::array<cv::Point2f, 2> top{pts[0], pts[1]};
  std::array<cv::Point2f, 2> bottom{pts[2], pts[3]};
  if (top[0].x > top[1].x) {
    std::swap(top[0], top[1]);
  }
  if (bottom[0].x > bottom[1].x) {
    std::swap(bottom[0], bottom[1]);
  }
  return {top[0], top[1], bottom[1], bottom[0]};
}

bool solvePose(
  const std::array<cv::Point2f, 4> & corners, double width_mm, double height_mm,
  const cv::Mat & camera_matrix, const cv::Mat & dist_coeffs,
  cv::Vec3d & rvec, cv::Vec3d & tvec,
  double & reproj_error, double & yaw_deg, double & pitch_deg, double & distance_mm)
{
  if (camera_matrix.empty() || camera_matrix.at<double>(0, 0) <= 0.0) {
    return false;
  }

  const auto object_points = objectPoints(width_mm, height_mm);
  std::vector<cv::Point3f> obj(object_points.begin(), object_points.end());
  std::vector<cv::Point2f> img(corners.begin(), corners.end());

  cv::Mat rvec_cv;
  cv::Mat tvec_cv;
  const bool ok =
    cv::solvePnP(obj, img, camera_matrix, dist_coeffs, rvec_cv, tvec_cv, false, cv::SOLVEPNP_IPPE);
  if (!ok || rvec_cv.empty() || tvec_cv.empty()) {
    return false;
  }

  std::vector<cv::Point2f> projected;
  cv::projectPoints(obj, rvec_cv, tvec_cv, camera_matrix, dist_coeffs, projected);
  double squared_error = 0.0;
  for (size_t i = 0; i < projected.size(); ++i) {
    const cv::Point2f delta = projected[i] - img[i];
    squared_error += static_cast<double>(delta.x * delta.x + delta.y * delta.y);
  }
  reproj_error = std::sqrt(squared_error / static_cast<double>(projected.size()));

  rvec = cv::Vec3d(rvec_cv.at<double>(0), rvec_cv.at<double>(1), rvec_cv.at<double>(2));
  tvec = cv::Vec3d(tvec_cv.at<double>(0), tvec_cv.at<double>(1), tvec_cv.at<double>(2));

  distance_mm = std::sqrt(tvec.dot(tvec));
  // 相机光学系：x 右、y 下、z 前
  yaw_deg = std::atan2(tvec[0], tvec[2]) * kDegPerRad;
  pitch_deg = -std::atan2(tvec[1], std::sqrt(tvec[0] * tvec[0] + tvec[2] * tvec[2])) * kDegPerRad;
  return true;
}

}  // namespace nr_vision_detector
