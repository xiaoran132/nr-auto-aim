#include "nr_vision_detector/yolo_detector.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <openvino/openvino.hpp>

#include "nr_vision_detector/armor_pose.hpp"
#include "nr_vision_detector/class_labels.hpp"

namespace nr_vision_detector
{
namespace
{
constexpr int kNumClasses = 38;
constexpr int kNumKeypoints = 4;
constexpr int kOutputChannels = 4 + kNumClasses + kNumKeypoints * 2;  // 50

struct Candidate
{
  float score{0.0F};
  int class_id{-1};
  cv::Rect2f box;
  std::array<cv::Point2f, 4> keypoints{};
};

float iou(const cv::Rect2f & a, const cv::Rect2f & b)
{
  const float inter = (a & b).area();
  const float uni = a.area() + b.area() - inter;
  return uni > 0.0F ? inter / uni : 0.0F;
}

bool colorMatches(const std::string & label_color, const std::string & enemy_color, bool keep_extra)
{
  if (enemy_color == "any") {
    return true;
  }
  if (label_color == "E" || label_color == "P") {
    return keep_extra;
  }
  if (enemy_color == "red") {
    return label_color == "R";
  }
  if (enemy_color == "blue") {
    return label_color == "B";
  }
  return true;
}
}  // namespace

class YoloDetector::Impl
{
public:
  Impl(const std::string & model_xml, const std::string & model_bin)
  {
    auto model = core.read_model(model_xml, model_bin);
    compiled = core.compile_model(model, "CPU");
    request = compiled.create_infer_request();
  }

  cv::Mat letterbox(const cv::Mat & src, int input_size, float & scale, int & pad_x, int & pad_y)
  {
    scale = std::min(
      static_cast<float>(input_size) / static_cast<float>(src.cols),
      static_cast<float>(input_size) / static_cast<float>(src.rows));
    const int new_w = static_cast<int>(std::round(static_cast<float>(src.cols) * scale));
    const int new_h = static_cast<int>(std::round(static_cast<float>(src.rows) * scale));

    cv::Mat resized;
    cv::resize(src, resized, cv::Size(new_w, new_h));

    pad_x = (input_size - new_w) / 2;
    pad_y = (input_size - new_h) / 2;
    cv::Mat canvas(input_size, input_size, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(canvas(cv::Rect(pad_x, pad_y, new_w, new_h)));
    return canvas;
  }

  ov::Core core;
  ov::CompiledModel compiled;
  ov::InferRequest request;
};

YoloDetector::YoloDetector(
  const std::string & model_xml, const std::string & model_bin, const DetectorParams & params)
: impl_(std::make_unique<Impl>(model_xml, model_bin)), params_(params)
{
}

YoloDetector::~YoloDetector() = default;

const DetectorParams & YoloDetector::params() const { return params_; }

std::vector<ArmorDetection> YoloDetector::detect(
  const cv::Mat & bgr, const cv::Mat & camera_matrix, const cv::Mat & dist_coeffs)
{
  std::vector<ArmorDetection> results;
  if (bgr.empty()) {
    return results;
  }

  float scale = 1.0F;
  int pad_x = 0;
  int pad_y = 0;
  const cv::Mat canvas = impl_->letterbox(bgr, params_.input_size, scale, pad_x, pad_y);

  cv::Mat blob = cv::dnn::blobFromImage(
    canvas, 1.0 / 255.0, cv::Size(params_.input_size, params_.input_size), cv::Scalar(), true,
    false);

  ov::Tensor input_tensor = impl_->request.get_input_tensor(0);
  if (input_tensor.get_byte_size() != blob.total() * blob.elemSize()) {
    throw std::runtime_error("模型输入张量大小与预处理结果不一致");
  }
  std::memcpy(input_tensor.data<float>(), blob.ptr<float>(), input_tensor.get_byte_size());

  impl_->request.infer();
  const ov::Tensor & output = impl_->request.get_output_tensor(0);
  const ov::Shape shape = output.get_shape();
  if (shape.size() != 3 || shape[0] != 1) {
    throw std::runtime_error("不支持的模型输出形状（期望 [1,50,N]）");
  }

  const size_t dim1 = shape[1];
  const size_t dim2 = shape[2];
  const bool channel_major = (dim1 == static_cast<size_t>(kOutputChannels));
  const bool candidate_major = (dim2 == static_cast<size_t>(kOutputChannels));
  if (!channel_major && !candidate_major) {
    throw std::runtime_error(
      "输出通道数不是 50（4 bbox + 38 class + 8 keypoint），请检查模型文件");
  }
  const int candidates = static_cast<int>(channel_major ? dim2 : dim1);
  const float * out = output.data<const float>();
  auto value_at = [&](int channel, int index) -> float {
    return channel_major ? out[channel * candidates + index]
                         : out[index * kOutputChannels + channel];
  };

  std::vector<Candidate> candidates_list;
  candidates_list.reserve(64);
  for (int i = 0; i < candidates; ++i) {
    float best_score = -1.0F;
    int best_class = -1;
    for (int c = 0; c < kNumClasses; ++c) {
      const float score = value_at(4 + c, i);
      if (score > best_score) {
        best_score = score;
        best_class = c;
      }
    }
    if (best_score < params_.score_threshold || best_class < 0) {
      continue;
    }

    const float cx = value_at(0, i);
    const float cy = value_at(1, i);
    const float w = value_at(2, i);
    const float h = value_at(3, i);

    Candidate cand;
    cand.score = best_score;
    cand.class_id = best_class;
    cand.box = cv::Rect2f(
      (cx - w * 0.5F - static_cast<float>(pad_x)) / scale,
      (cy - h * 0.5F - static_cast<float>(pad_y)) / scale, w / scale, h / scale);

    for (int k = 0; k < kNumKeypoints; ++k) {
      const float kx = (value_at(4 + kNumClasses + k * 2, i) - static_cast<float>(pad_x)) / scale;
      const float ky =
        (value_at(4 + kNumClasses + k * 2 + 1, i) - static_cast<float>(pad_y)) / scale;
      cand.keypoints[static_cast<size_t>(k)] = cv::Point2f(kx, ky);
    }
    candidates_list.push_back(cand);
  }

  // 按置信度降序 + 类内 NMS
  std::sort(
    candidates_list.begin(), candidates_list.end(),
    [](const Candidate & a, const Candidate & b) { return a.score > b.score; });
  std::vector<Candidate> kept;
  for (const auto & cand : candidates_list) {
    bool suppressed = false;
    for (const auto & other : kept) {
      if (other.class_id == cand.class_id && iou(other.box, cand.box) > params_.nms_threshold) {
        suppressed = true;
        break;
      }
    }
    if (!suppressed) {
      kept.push_back(cand);
    }
  }

  const bool pose_available = !camera_matrix.empty() && camera_matrix.total() == 9 &&
                              camera_matrix.at<double>(0, 0) > 0.0;

  for (const auto & cand : kept) {
    const std::string label = labelOf(cand.class_id);
    const ArmorLabelInfo info = parseLabel(label);
    if (!colorMatches(info.color, params_.enemy_color, params_.keep_extra_colors)) {
      continue;
    }

    ArmorDetection det;
    det.class_id = cand.class_id;
    det.label = label;
    det.color = info.color;
    det.name = info.name;
    det.type = info.type;
    det.score = cand.score;
    det.box = cand.box;
    det.keypoints_raw = cand.keypoints;
    det.corners = sortCorners(cand.keypoints);

    if (pose_available) {
      const double width_mm =
        (info.type == "large") ? params_.large_armor_width : params_.small_armor_width;
      double reproj = 0.0;
      double yaw = 0.0;
      double pitch = 0.0;
      double distance = 0.0;
      cv::Vec3d rvec;
      cv::Vec3d tvec;
      if (solvePose(
            det.corners, width_mm, params_.armor_height, camera_matrix, dist_coeffs, rvec, tvec,
            reproj, yaw, pitch, distance)) {
        if (reproj <= params_.max_reproj_error && distance >= params_.min_distance_mm &&
          distance <= params_.max_distance_mm) {
          det.pose_valid = true;
          det.rvec = rvec;
          det.tvec = tvec;
          det.reproj_error = reproj;
          det.yaw_deg = yaw;
          det.pitch_deg = pitch;
          det.distance_mm = distance;
        }
      }
    }

    results.push_back(det);
  }

  std::sort(results.begin(), results.end(), [](const ArmorDetection & a, const ArmorDetection & b) {
    return a.score > b.score;
  });
  return results;
}

}  // namespace nr_vision_detector
