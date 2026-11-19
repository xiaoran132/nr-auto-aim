#pragma once

#include <array>
#include <string>

#include <opencv2/core.hpp>

namespace nr_vision_detector
{
// 物方点（mm，装甲板平面，x 右 / y 下 / z 平面法向），顺序：左上、右上、右下、左下
std::array<cv::Point3f, 4> objectPoints(double width_mm, double height_mm);

// 把模型输出的 4 个关键点排序为：左上、右上、右下、左下
// 与 sp_vision_25 / ros2 分支一致的「上两点按 x 排、下两点按 x 排」策略
std::array<cv::Point2f, 4> sortCorners(const std::array<cv::Point2f, 4> & corners);

// PnP：solvePnP(IPPE) + 重投影误差 + 由 tvec 解出 yaw/pitch（度）
bool solvePose(
  const std::array<cv::Point2f, 4> & corners, double width_mm, double height_mm,
  const cv::Mat & camera_matrix, const cv::Mat & dist_coeffs,
  cv::Vec3d & rvec, cv::Vec3d & tvec,
  double & reproj_error, double & yaw_deg, double & pitch_deg, double & distance_mm);

}  // namespace nr_vision_detector
