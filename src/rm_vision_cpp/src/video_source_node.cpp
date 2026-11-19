#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/image.hpp>

namespace rm_vision_cpp
{

class VideoSourceNode : public rclcpp::Node
{
public:
  VideoSourceNode()
  : Node("video_source")
  {
    declare_parameter("source", "0");
    declare_parameter("fps", 30.0);
    declare_parameter("frame_id", "camera_optical_frame");
    declare_parameter("loop", true);
    declare_parameter("fx", 1408.0);
    declare_parameter("fy", 1408.0);
    declare_parameter("cx", 640.0);
    declare_parameter("cy", 512.0);

    source_ = get_parameter("source").as_string();
    fps_ = get_parameter("fps").as_double();
    frame_id_ = get_parameter("frame_id").as_string();
    loop_ = get_parameter("loop").as_bool();

    image_pub_ = create_publisher<sensor_msgs::msg::Image>("image_raw", 10);
    info_pub_ = create_publisher<sensor_msgs::msg::CameraInfo>("camera_info", 10);

    if (std::filesystem::is_directory(source_)) {
      for (const auto & entry : std::filesystem::directory_iterator(source_)) {
        const auto ext = entry.path().extension().string();
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".webp") {
          image_files_.push_back(entry.path().string());
        }
      }
      std::sort(image_files_.begin(), image_files_.end());
      if (image_files_.empty()) {
        throw std::runtime_error("目录里没有图片: " + source_);
      }
      RCLCPP_INFO(get_logger(), "图片序列模式: %zu 张", image_files_.size());
    } else {
      const bool is_device = !source_.empty() &&
        source_.find_first_not_of("0123456789") == std::string::npos;
      if (is_device) {
        capture_.open(std::stoi(source_));
      } else {
        capture_.open(source_);
      }
      if (!capture_.isOpened()) {
        throw std::runtime_error("无法打开视频源: " + source_);
      }
      RCLCPP_INFO(get_logger(), "视频模式: %s", source_.c_str());
    }

    const double period = 1.0 / std::max(fps_, 1.0);
    timer_ = create_wall_timer(
      std::chrono::duration<double>(period),
      std::bind(&VideoSourceNode::onTimer, this));
  }

private:
  void onTimer()
  {
    cv::Mat frame;
    if (!image_files_.empty()) {
      const std::string path = image_files_[image_index_ % image_files_.size()];
      frame = cv::imread(path);
      ++image_index_;
    } else {
      capture_ >> frame;
      if (frame.empty() && loop_) {
        capture_.set(cv::CAP_PROP_POS_FRAMES, 0);
        capture_ >> frame;
      }
    }

    if (frame.empty()) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "读取帧失败");
      return;
    }

    const auto stamp = now();
    cv_bridge::CvImage image_msg;
    image_msg.header.stamp = stamp;
    image_msg.header.frame_id = frame_id_;
    image_msg.encoding = sensor_msgs::image_encodings::BGR8;
    image_msg.image = frame;
    image_pub_->publish(*image_msg.toImageMsg());

    sensor_msgs::msg::CameraInfo info;
    info.header.stamp = stamp;
    info.header.frame_id = frame_id_;
    info.width = static_cast<uint32_t>(frame.cols);
    info.height = static_cast<uint32_t>(frame.rows);
    info.distortion_model = "plumb_bob";
    info.d = {0.0, 0.0, 0.0, 0.0, 0.0};
    info.k = {
      get_parameter("fx").as_double(), 0.0, get_parameter("cx").as_double(),
      0.0, get_parameter("fy").as_double(), get_parameter("cy").as_double(),
      0.0, 0.0, 1.0};
    info.p = {info.k[0], 0.0, info.k[2], 0.0, 0.0, info.k[4], info.k[5], 0.0, 0.0, 0.0, 1.0, 0.0};
    info_pub_->publish(info);
  }

  std::string source_;
  std::string frame_id_;
  double fps_{30.0};
  bool loop_{true};
  cv::VideoCapture capture_;
  std::vector<std::string> image_files_;
  size_t image_index_{0};

  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
  rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr info_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace rm_vision_cpp

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rm_vision_cpp::VideoSourceNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
