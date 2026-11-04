#ifndef RM_VISION_CPP__ARMOR_DETECT_HPP_
#define RM_VISION_CPP__ARMOR_DETECT_HPP_

#include <array>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>

namespace rm_vision_cpp
{

/// 装甲板的物理几何（单位 mm）
struct ArmorGeometry
{
  std::string name;
  double width;
  double height;
};

extern const ArmorGeometry kSmallArmor;
extern const ArmorGeometry kLargeArmor;

/// 检测参数（与 Python 版一一对应）
struct DetectorParams
{
  std::string color{"red"};
  double color_diff_thresh{45.0};
  double adaptive_ratio{0.45};
  double max_color_diff_thresh{130.0};
  double min_brightness{110.0};
  double min_light_len_ratio{0.008};
  double max_light_len_ratio{0.75};
  double min_light_aspect{1.5};
  double max_light_aspect{14.0};
  double max_light_tilt_deg{40.0};
  double min_light_fill{0.40};
  double max_length_ratio_diff{0.35};
  double max_tilt_diff_deg{12.0};
  double min_width_ratio{1.25};
  double max_width_ratio{5.6};
  double max_perpendicular_cos{0.42};
  double max_y_offset_ratio{0.70};
  double min_y_overlap_ratio{0.25};
  double large_threshold_ratio{3.05};
  double min_distance_mm{300.0};
  double max_distance_mm{20000.0};
};

/// 单根灯条
struct LightBar
{
  cv::Point2f top;
  cv::Point2f bottom;
  cv::Point2f center;
  double length{0.0};
  double thickness{0.0};
  double tilt_deg{0.0};
  cv::Point2f axis;  ///< 长轴单位向量
};

/// 单块装甲板的检测结果
struct Armor
{
  std::string type;  ///< "small" / "large"
  double width_ratio{0.0};
  std::array<cv::Point2f, 4> corners;  ///< 左下、左上、右上、右下
  cv::Vec3d rvec;
  cv::Vec3d tvec;
  double yaw{0.0};        ///< 度，右为正
  double pitch{0.0};      ///< 度，上为正
  double distance_mm{0.0};
  double reproj_error{0.0};
};

/// 纯算法类：不依赖 rclcpp，可以离线测试
class ArmorDetector
{
public:
  explicit ArmorDetector(const DetectorParams & params = DetectorParams{});

  std::vector<Armor> detect(
    const cv::Mat & image_bgr,
    const cv::Mat & camera_matrix,
    const cv::Mat & dist_coeffs);

  const DetectorParams & params() const {return params_;}

private:
  cv::Mat buildColorMask(const cv::Mat & image_bgr) const;
  std::vector<LightBar> findLightBars(const cv::Mat & mask, const cv::Size & image_size) const;
  bool pairCost(const LightBar & left, const LightBar & right, double & width_ratio) const;
  std::vector<std::pair<LightBar, LightBar>> pairLightBars(
    const std::vector<LightBar> & bars) const;

  DetectorParams params_;
};

/// 4 个物方点：左灯条下 -> 左灯条上 -> 右灯条上 -> 右灯条下
std::array<cv::Point3f, 4> armorObjectPoints(const ArmorGeometry & armor);

/// 由 tvec 解出云台角度（度）
void anglesFromTvec(const cv::Vec3d & tvec, double & yaw_deg, double & pitch_deg);

/// 在图像上画出检测框
cv::Mat drawDetections(const cv::Mat & image, const std::vector<Armor> & armors);

}  // namespace rm_vision_cpp

#endif  // RM_VISION_CPP__ARMOR_DETECT_HPP_
