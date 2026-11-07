// 相机节点：用 nr_vision_io（包装 main 的 io::Camera）发布 /image_raw + /camera_info
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/image_encodings.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/image.hpp>

#include "nr_vision_io/camera_driver.hpp"

namespace rm_vision_cpp
{
class CameraNode : public rclcpp::Node
{
public:
  CameraNode()
  : Node("camera_node")
  {
    const std::string repo_root = NR_REPO_ROOT;

    camera_config_ =
      this->declare_parameter<std::string>("camera_config", repo_root + "/configs/camera.yaml");
    calibration_file_ = this->declare_parameter<std::string>(
      "calibration_file", repo_root + "/configs/calibration.yaml");
    video_file_ = this->declare_parameter<std::string>("video_file", "");
    frame_id_ = this->declare_parameter<std::string>("frame_id", "camera_optical_frame");
    fps_ = this->declare_parameter<double>("fps", 30.0);
    const bool use_sensor_qos = this->declare_parameter<bool>("use_sensor_data_qos", true);

    if (camera_config_.empty()) {
      camera_config_ = repo_root + "/configs/camera.yaml";
    }
    if (calibration_file_.empty()) {
      calibration_file_ = repo_root + "/configs/calibration.yaml";
    }

    rclcpp::QoS qos(rclcpp::KeepLast(10));
    if (use_sensor_qos) {
      qos = rclcpp::SensorDataQoS();
    }
    image_pub_ = this->create_publisher<sensor_msgs::msg::Image>("image_raw", qos);
    info_pub_ = this->create_publisher<sensor_msgs::msg::CameraInfo>("camera_info", qos);

    if (!video_file_.empty()) {
      video_.open(video_file_);
      if (!video_.isOpened()) {
        throw std::runtime_error("无法打开视频文件: " + video_file_);
      }
      RCLCPP_INFO(this->get_logger(), "视频回放模式: %s", video_file_.c_str());
    } else {
      driver_ = std::make_unique<nr_vision_io::CameraDriver>(camera_config_, calibration_file_);
      if (!driver_->isOpened()) {
        throw std::runtime_error("相机打开失败，请检查 " + camera_config_);
      }
      RCLCPP_INFO(
        this->get_logger(), "相机已打开: %s（标定: %s）", camera_config_.c_str(),
        calibration_file_.c_str());
    }

    const auto period = std::chrono::duration<double>(1.0 / std::max(fps_, 1.0));
    timer_ = this->create_wall_timer(period, std::bind(&CameraNode::captureOnce, this));
  }

private:
  void captureOnce()
  {
    cv::Mat frame;
    if (video_.isOpened()) {
      video_ >> frame;
      if (frame.empty()) {
        RCLCPP_WARN_THROTTLE(
          this->get_logger(), *this->get_clock(), 2000, "视频读到末尾（可换视频或重启）");
        return;
      }
    } else {
      if (!driver_ || !driver_->read(frame) || frame.empty()) {
        RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "取流失败");
        return;
      }
    }

    const rclcpp::Time stamp = this->now();

    cv_bridge::CvImage image_msg;
    image_msg.header.stamp = stamp;
    image_msg.header.frame_id = frame_id_;
    image_msg.encoding = sensor_msgs::image_encodings::BGR8;
    image_msg.image = frame;
    image_pub_->publish(*image_msg.toImageMsg());

    sensor_msgs::msg::CameraInfo info;
    info.header.stamp = stamp;
    bool calibrated = false;
    if (driver_) {
      calibrated = driver_->fillCameraInfo(info, frame_id_);
    }
    if (!calibrated && !calib_warned_) {
      calib_warned_ = true;
      RCLCPP_WARN(
        this->get_logger(),
        "标定缺失：/camera_info 的 K 为 0，检测节点将跳过位姿解算。"
        "先按 README 用棋盘格标定并写入 configs/calibration.yaml");
    }
    info.width = static_cast<uint32_t>(frame.cols);
    info.height = static_cast<uint32_t>(frame.rows);
    info_pub_->publish(info);
  }

  std::string camera_config_;
  std::string calibration_file_;
  std::string video_file_;
  std::string frame_id_;
  double fps_{30.0};
  bool calib_warned_{false};

  cv::VideoCapture video_;
  std::unique_ptr<nr_vision_io::CameraDriver> driver_;

  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
  rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr info_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace rm_vision_cpp

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<rm_vision_cpp::CameraNode>();
    rclcpp::spin(node);
  } catch (const std::exception & e) {
    std::fprintf(stderr, "camera_node 启动失败: %s\n", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
