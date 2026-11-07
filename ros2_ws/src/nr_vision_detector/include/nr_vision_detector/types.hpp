#pragma once

#include <array>
#include <string>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace nr_vision_detector
{
struct DetectorParams
{
  int input_size{640};
  float score_threshold{0.5F};
  float nms_threshold{0.45F};

  // 装甲板物理尺寸（mm）：小/大装甲板 = 灯条间距 x 灯条长度
  double small_armor_width{135.0};
  double large_armor_width{225.0};
  double armor_height{55.0};

  double max_reproj_error{5.0};   // px，超过则丢弃该装甲板
  double min_distance_mm{300.0};
  double max_distance_mm{20000.0};

  std::string enemy_color{"red"};  // red / blue / any
  bool keep_extra_colors{false};   // 是否保留 E(灭灯)/P(紫色) 类
};

struct ArmorDetection
{
  int class_id{-1};
  std::string label;
  std::string color;
  std::string name;
  std::string type;  // small / large
  float score{0.0F};

  cv::Rect2f box;
  std::array<cv::Point2f, 4> corners{};  // 排序后：左上、右上、右下、左下
  std::array<cv::Point2f, 4> keypoints_raw{};  // 模型原始输出（未排序）

  bool pose_valid{false};
  cv::Vec3d rvec{0.0, 0.0, 0.0};
  cv::Vec3d tvec{0.0, 0.0, 0.0};
  double yaw_deg{0.0};     // 右为正
  double pitch_deg{0.0};   // 上为正
  double distance_mm{0.0};
  double reproj_error{0.0};
};

}  // namespace nr_vision_detector
