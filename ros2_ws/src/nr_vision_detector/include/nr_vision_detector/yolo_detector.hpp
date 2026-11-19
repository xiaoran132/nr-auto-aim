#pragma once

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "nr_vision_detector/types.hpp"

namespace nr_vision_detector
{
// YOLO11（38 类 + 4 关键点）检测 + PnP，纯算法、不依赖 ROS。
// OpenVINO 细节隐藏在 Impl 里，头文件不暴露 OpenVINO 依赖。
class YoloDetector
{
public:
  YoloDetector(
    const std::string & model_xml, const std::string & model_bin,
    const DetectorParams & params = DetectorParams{});
  ~YoloDetector();

  YoloDetector(const YoloDetector &) = delete;
  YoloDetector & operator=(const YoloDetector &) = delete;

  // camera_matrix / dist_coeffs 为空或无效时只做检测、不做位姿（pose_valid=false）
  std::vector<ArmorDetection> detect(
    const cv::Mat & bgr, const cv::Mat & camera_matrix, const cv::Mat & dist_coeffs);

  const DetectorParams & params() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
  DetectorParams params_;
};

}  // namespace nr_vision_detector
