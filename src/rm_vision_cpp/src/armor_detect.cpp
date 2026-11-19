#include "rm_vision_cpp/armor_detect.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

namespace rm_vision_cpp
{
///装甲板大小
const ArmorGeometry kSmallArmor{"small", 135.0, 55.0};
const ArmorGeometry kLargeArmor{"large", 225.0, 55.0};
///匿名命名空间工具函数
namespace
{

/// 对 CV_8U 图像求分位数（不复制整张图也可以，这里为了简单直接拷贝）
double percentileU8(const cv::Mat & mat, double p)
{
  CV_Assert(mat.type() == CV_8UC1);
  cv::Mat flat = mat.isContinuous() ? mat : mat.clone();
  std::vector<uchar> values(flat.data, flat.data + flat.total());
  if (values.empty()) {
    return 0.0;
  }
  const size_t index = static_cast<size_t>(p * static_cast<double>(values.size() - 1));
  std::nth_element(values.begin(), values.begin() + index, values.end());
  return static_cast<double>(values[index]);
}

}  // namespace


ArmorDetector::ArmorDetector(const DetectorParams & params)
: params_(params)
{
}

///3D 模型点
std::array<cv::Point3f, 4> armorObjectPoints(const ArmorGeometry & armor)
{
  const float hw = static_cast<float>(armor.width / 2.0);
  const float hh = static_cast<float>(armor.height / 2.0);
  return {
    cv::Point3f(-hw, +hh, 0.0F),
    cv::Point3f(-hw, -hh, 0.0F),
    cv::Point3f(+hw, -hh, 0.0F),
    cv::Point3f(+hw, +hh, 0.0F),
  };
}

///平移向量算偏航/俯仰角
void anglesFromTvec(const cv::Vec3d & tvec, double & yaw_deg, double & pitch_deg)
{
  const double x = tvec[0];
  const double y = tvec[1];
  const double z = tvec[2];
  yaw_deg = std::atan2(x, z) * 180.0 / CV_PI;            //偏航
  pitch_deg = std::atan2(-y, std::hypot(x, z)) * 180.0 / CV_PI;          //俯仰
}

//颜色阈值分割（三通道）
cv::Mat ArmorDetector::buildColorMask(const cv::Mat & image_bgr) const
{
  CV_Assert(image_bgr.type() == CV_8UC3);

  std::vector<cv::Mat> channels;
  cv::split(image_bgr, channels);  // B, G, R

  cv::Mat diff;
  if (params_.color == "red") {
    cv::Mat gb;
    cv::max(channels[1], channels[2], gb);
    cv::subtract(channels[2], gb, diff);
  } else {
    cv::Mat gr;
    cv::max(channels[1], channels[0], gr);
    cv::subtract(channels[0], gr, diff);
  }
//这里分支有点小问题（待解决）
  cv::Mat brightness;
  cv::max(channels[1], channels[2], brightness);
  cv::max(brightness, channels[0], brightness);

  const double scale = percentileU8(diff, 0.995);
  const double threshold = std::min(
    std::max(params_.adaptive_ratio * scale, params_.color_diff_thresh),
    params_.max_color_diff_thresh);

  cv::Mat mask_color;
  cv::Mat mask_bright;
  cv::threshold(diff, mask_color, threshold, 255, cv::THRESH_BINARY);
  cv::threshold(brightness, mask_bright, params_.min_brightness, 255, cv::THRESH_BINARY);

  cv::Mat mask;
  cv::bitwise_and(mask_color, mask_bright, mask);

  const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
  cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
  return mask;
}

std::vector<LightBar> ArmorDetector::findLightBars(
  const cv::Mat & mask, const cv::Size & image_size) const
{
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);      //提取灯条 mask

  std::vector<LightBar> bars;
  for (const auto & contour : contours) {
    const double area = cv::contourArea(contour);
    if (area < 20.0) {
      continue;
    }            //早点

    const cv::RotatedRect rect = cv::minAreaRect(contour);        //矩形旋转
    if (rect.size.width < 2.0F || rect.size.height < 2.0F) {
      continue;
    }

    cv::Point2f pts[4];
    rect.points(pts);      //角

    const cv::Point2f edge_a = pts[1] - pts[0];
    const cv::Point2f edge_b = pts[2] - pts[1];
    const auto point_norm = [](const cv::Point2f & p) {
        return std::hypot(static_cast<double>(p.x), static_cast<double>(p.y));
      };
    const cv::Point2f long_vec = (point_norm(edge_a) >= point_norm(edge_b)) ? edge_a : edge_b;
    const double length = point_norm(long_vec);
    if (length < 2.0) {
      continue;
    }
    const double thickness = area / length;        // 用面积/长度估厚度
    if (thickness < 1.0) {
      continue;
    }

    const double rect_area = static_cast<double>(rect.size.width) * rect.size.height;
    const double fill = (rect_area > 1e-6) ? area / rect_area : 0.0;        //轮廓面积 / 矩形面积
    const cv::Point2f axis = long_vec / static_cast<float>(length);            //灯条轴向单位向量
    const double tilt = std::atan2(std::abs(axis.x), std::abs(axis.y)) * 180.0 / CV_PI;            //竖直方向的倾角

    if (length < params_.min_light_len_ratio * image_size.height ||
      length > params_.max_light_len_ratio * image_size.height)
    {
      continue;
    }
    const double aspect = length / thickness;
    if (aspect < params_.min_light_aspect || aspect > params_.max_light_aspect) {
      continue;
    }
    if (tilt > params_.max_light_tilt_deg || fill < params_.min_light_fill) {
      continue;
    }

    // 取两条短边的中点，而不是外接矩形的角点
    std::vector<cv::Point2f> mids;
    for (int k = 0; k < 4; ++k) {
      const cv::Point2f edge = pts[(k + 1) % 4] - pts[k];
      const double edge_norm = point_norm(edge);
      if (edge_norm < 1e-6) {
        continue;
      }
      if (std::abs(edge.dot(axis) / edge_norm) < 0.5) {              // 与长轴夹角>60°的边=短边
        mids.push_back((pts[k] + pts[(k + 1) % 4]) * 0.5F);
      }
    }
    if (mids.size() < 2) {
      continue;
    }

    LightBar bar;
    if (mids[0].y <= mids[1].y) {
      bar.top = mids[0];
      bar.bottom = mids[1];
    } else {
      bar.top = mids[1];
      bar.bottom = mids[0];
    }
    bar.center = rect.center;
    bar.length = point_norm(bar.bottom - bar.top);
    bar.thickness = thickness;
    bar.tilt_deg = tilt;
    bar.axis = axis;
    bars.push_back(bar);
  }
  return bars;
}

//判断两根灯条成甲板
bool ArmorDetector::pairCost(
  const LightBar & left, const LightBar & right, double & width_ratio) const
{
  const double mean_len = 0.5 * (left.length + right.length);
  if (mean_len <= 1e-6) {
    return false;
  }
  if (std::abs(left.length - right.length) / mean_len > params_.max_length_ratio_diff) {
    return false;          // 两根灯条长度应相近。1
  }
  if (std::abs(left.tilt_deg - right.tilt_deg) > params_.max_tilt_diff_deg) {
    return false;               //平行.2
  }

  const cv::Point2f connector = right.center - left.center;
  const double distance = std::hypot(
    static_cast<double>(connector.x), static_cast<double>(connector.y));
  width_ratio = distance / mean_len;
  if (width_ratio < params_.min_width_ratio || width_ratio > params_.max_width_ratio) {
    return false;                  //两条的中心距 / 平均灯条长。3
  }

  const cv::Point2f unit = connector / static_cast<float>(std::max(distance, 1e-6));
  if (std::abs(unit.dot(left.axis)) > params_.max_perpendicular_cos) {
    return false;                      //连线应大致垂直于灯条。4
  }
  if (std::abs(left.center.y - right.center.y) / mean_len > params_.max_y_offset_ratio) {
    return false;                      //高度差不能太大。5
  }

  const double left_lo = std::min(left.top.y, left.bottom.y);
  const double left_hi = std::max(left.top.y, left.bottom.y);
  const double right_lo = std::min(right.top.y, right.bottom.y);
  const double right_hi = std::max(right.top.y, right.bottom.y);
  const double overlap = std::min(left_hi, right_hi) - std::max(left_lo, right_lo);
  if (overlap < params_.min_y_overlap_ratio * mean_len) {
    return false;                    //数直的重叠。6
  }
  return true;
}

//pairLightBars的配对（可能有点问题）
std::vector<std::pair<LightBar, LightBar>> ArmorDetector::pairLightBars(
  const std::vector<LightBar> & bars) const
{
  struct Candidate
  {
    double cost;
    double width_ratio;
    LightBar left;
    LightBar right;
  };          //拷贝

  std::vector<Candidate> candidates;
  for (size_t i = 0; i < bars.size(); ++i) {
    for (size_t j = i + 1; j < bars.size(); ++j) {
      LightBar left = bars[i];
      LightBar right = bars[j];
      if (left.center.x > right.center.x) {
        std::swap(left, right);
      }
      double width_ratio = 0.0;
      if (!pairCost(left, right, width_ratio)) {
        continue;
      }
      candidates.push_back(Candidate{std::abs(width_ratio - 3.0), width_ratio, left, right});
    }
  }              //枚举筛选

  std::sort(
    candidates.begin(), candidates.end(),
    [](const Candidate & a, const Candidate & b) {return a.cost < b.cost;});            //排序

  std::vector<std::pair<LightBar, LightBar>> pairs;
  std::vector<bool> used(bars.size(), false);

  // 用指针偏移判断某个候选用的是哪两根灯条
  for (const auto & candidate : candidates) {
    size_t left_index = bars.size();
    size_t right_index = bars.size();
    for (size_t k = 0; k < bars.size(); ++k) {
      if (std::hypot(
          static_cast<double>(bars[k].center.x - candidate.left.center.x),
          static_cast<double>(bars[k].center.y - candidate.left.center.y)) < 1e-3)
      {
        left_index = k;
      }
      if (std::hypot(
          static_cast<double>(bars[k].center.x - candidate.right.center.x),
          static_cast<double>(bars[k].center.y - candidate.right.center.y)) < 1e-3)
      {
        right_index = k;
      }
    }
    if (left_index >= bars.size() || right_index >= bars.size()) {
      continue;
    }
    if (used[left_index] || used[right_index]) {
      continue;
    }
    used[left_index] = true;
    used[right_index] = true;
    pairs.emplace_back(candidate.left, candidate.right);
  }
  return pairs;
}

//主流程
std::vector<Armor> ArmorDetector::detect(
  const cv::Mat & image_bgr,
  const cv::Mat & camera_matrix,
  const cv::Mat & dist_coeffs)
{
  std::vector<Armor> results;
  const cv::Mat mask = buildColorMask(image_bgr);
  const auto bars = findLightBars(mask, image_bgr.size());
  const auto pairs = pairLightBars(bars);

  for (const auto & pair : pairs) {
    const LightBar & left = pair.first;
    const LightBar & right = pair.second;
    const double width_ratio = std::hypot(
      static_cast<double>(right.center.x - left.center.x),
      static_cast<double>(right.center.y - left.center.y)) /
      (0.5 * (left.length + right.length));
    //
    Armor armor;
    armor.type = (width_ratio >= params_.large_threshold_ratio) ? "large" : "small";
    armor.width_ratio = width_ratio;
    armor.corners = {left.bottom, left.top, right.top, right.bottom};

    //pnp
    const ArmorGeometry & geometry = (armor.type == "large") ? kLargeArmor : kSmallArmor;
    const auto object_points = armorObjectPoints(geometry);

    std::vector<cv::Point3f> obj(object_points.begin(), object_points.end());
    std::vector<cv::Point2f> img(armor.corners.begin(), armor.corners.end());

    cv::Mat rvec;
    cv::Mat tvec;
    const bool ok = cv::solvePnP(
      obj, img, camera_matrix, dist_coeffs, rvec, tvec, false, cv::SOLVEPNP_IPPE);
    if (!ok) {
      continue;
    }

    std::vector<cv::Point2f> projected;
    cv::projectPoints(obj, rvec, tvec, camera_matrix, dist_coeffs, projected);

    double squared_error = 0.0;
    for (size_t k = 0; k < projected.size(); ++k) {
      const cv::Point2f delta = projected[k] - img[k];
      squared_error += delta.x * delta.x + delta.y * delta.y;
    }
    armor.reproj_error = std::sqrt(squared_error / static_cast<double>(projected.size()));
//欧氏距离
    armor.rvec = cv::Vec3d(rvec.at<double>(0), rvec.at<double>(1), rvec.at<double>(2));
    armor.tvec = cv::Vec3d(tvec.at<double>(0), tvec.at<double>(1), tvec.at<double>(2));
    armor.distance_mm = std::sqrt(armor.tvec.dot(armor.tvec));
//过滤
    if (armor.distance_mm < params_.min_distance_mm ||
      armor.distance_mm > params_.max_distance_mm)
    {
      continue;
    }

    anglesFromTvec(armor.tvec, armor.yaw, armor.pitch);
    results.push_back(armor);
  }
  return results;
}

//可视化
cv::Mat drawDetections(const cv::Mat & image, const std::vector<Armor> & armors)
{
  cv::Mat canvas = image.clone();
  for (const auto & armor : armors) {
    std::vector<cv::Point> points;
    points.reserve(4);
    cv::Point2f center(0.0F, 0.0F);
    for (const auto & corner : armor.corners) {
      points.emplace_back(
        static_cast<int>(std::lround(corner.x)),
        static_cast<int>(std::lround(corner.y)));
      center += corner;
    }
    center *= 0.25F;
    cv::polylines(canvas, points, true, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);

    const std::string text = armor.type + " " +
      cv::format("%.2fm yaw=%.1f", armor.distance_mm / 1000.0, armor.yaw);
    cv::putText(
      canvas, text,
      cv::Point(static_cast<int>(center.x) - 80, std::max(15, static_cast<int>(center.y) - 10)),
      cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
  }
  return canvas;
}

}  // namespace rm_vision_cpp
