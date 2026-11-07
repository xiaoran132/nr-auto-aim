#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/opencv.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/image_encodings.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

#include "nr_vision_detector/types.hpp"
#include "nr_vision_detector/yolo_detector.hpp"
#include "rm_vision_interfaces/msg/armor.hpp"
#include "rm_vision_interfaces/msg/armors.hpp"
// 检测节点：YOLO11 + 关键点排序 + PnP（nr_vision_detector 薄壳）

namespace rm_vision_cpp
{
class DetectorNode : public rclcpp::Node
{
public:
  DetectorNode()
  : Node("armor_detector")
  {
    const std::string repo_root = NR_REPO_ROOT;

    std::string model_xml = this->declare_parameter<std::string>("model_xml", "");
    std::string model_bin = this->declare_parameter<std::string>("model_bin", "");
    if (model_xml.empty()) {
      model_xml = repo_root + "/assets/yolo11.xml";
    }
    if (model_bin.empty()) {
      model_bin = repo_root + "/assets/yolo11.bin";
    }

    nr_vision_detector::DetectorParams params;
    params.score_threshold =
      static_cast<float>(this->declare_parameter<double>("score_threshold", 0.5));
    params.nms_threshold =
      static_cast<float>(this->declare_parameter<double>("nms_threshold", 0.45));
    params.enemy_color = this->declare_parameter<std::string>("enemy_color", "red");
    params.keep_extra_colors = this->declare_parameter<bool>("keep_extra_colors", false);
    params.small_armor_width = this->declare_parameter<double>("small_armor_width", 135.0);
    params.large_armor_width = this->declare_parameter<double>("large_armor_width", 225.0);
    params.armor_height = this->declare_parameter<double>("armor_height", 55.0);
    params.max_reproj_error = this->declare_parameter<double>("max_reproj_error", 5.0);
    params.min_distance_mm = this->declare_parameter<double>("min_distance_mm", 300.0);
    params.max_distance_mm = this->declare_parameter<double>("max_distance_mm", 20000.0);
    debug_ = this->declare_parameter<bool>("debug", true);

    detector_ = std::make_unique<nr_vision_detector::YoloDetector>(model_xml, model_bin, params);

    rclcpp::QoS qos(rclcpp::KeepLast(10));
    qos = rclcpp::SensorDataQoS();
    image_sub_ = this->create_subscription<sensor_msgs::msg::Image>(
      "image_raw", qos, std::bind(&DetectorNode::onImage, this, std::placeholders::_1));
    info_sub_ = this->create_subscription<sensor_msgs::msg::CameraInfo>(
      "camera_info", qos, std::bind(&DetectorNode::onCameraInfo, this, std::placeholders::_1));
    armors_pub_ = this->create_publisher<rm_vision_interfaces::msg::Armors>("armors", qos);
    if (debug_) {
      debug_pub_ = image_transport::create_publisher(this, "~/debug_image");
    }

    RCLCPP_INFO(
      this->get_logger(), "检测节点启动: %s（enemy_color=%s）", model_xml.c_str(),
      params.enemy_color.c_str());
  }

private:
  void onCameraInfo(const sensor_msgs::msg::CameraInfo::SharedPtr msg)
  {
    if (msg->k[0] == 0.0) {
      return;
    }
    camera_matrix_ = cv::Mat(3, 3, CV_64F);
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        camera_matrix_.at<double>(r, c) = msg->k[static_cast<size_t>(r * 3 + c)];
      }
    }
    const size_t dist_size = msg->d.empty() ? 5 : msg->d.size();
    dist_coeffs_ = cv::Mat(static_cast<int>(dist_size), 1, CV_64F, cv::Scalar(0.0));
    for (size_t i = 0; i < msg->d.size(); ++i) {
      dist_coeffs_.at<double>(static_cast<int>(i), 0) = msg->d[i];
    }
    if (!calib_logged_) {
      calib_logged_ = true;
      RCLCPP_INFO(
        this->get_logger(), "收到相机内参: fx=%.2f fy=%.2f cx=%.2f cy=%.2f",
        camera_matrix_.at<double>(0, 0), camera_matrix_.at<double>(1, 1),
        camera_matrix_.at<double>(0, 2), camera_matrix_.at<double>(1, 2));
    }
  }

  void onImage(const sensor_msgs::msg::Image::SharedPtr msg)
  {
    cv_bridge::CvImageConstPtr cv_ptr;
    try {
      cv_ptr = cv_bridge::toCvShare(msg, sensor_msgs::image_encodings::BGR8);
    } catch (const cv_bridge::Exception & ex) {
      RCLCPP_ERROR(this->get_logger(), "图像转换失败: %s", ex.what());
      return;
    }

    std::vector<nr_vision_detector::ArmorDetection> detections;
    try {
      detections = detector_->detect(cv_ptr->image, camera_matrix_, dist_coeffs_);
    } catch (const std::exception & ex) {
      RCLCPP_ERROR_THROTTLE(
        this->get_logger(), *this->get_clock(), 2000, "推理失败: %s", ex.what());
      return;
    }

    rm_vision_interfaces::msg::Armors armors_msg;
    armors_msg.header = msg->header;

    const float image_cx = static_cast<float>(cv_ptr->image.cols) * 0.5F;
    const float image_cy = static_cast<float>(cv_ptr->image.rows) * 0.5F;

    for (const auto & det : detections) {
      if (!det.pose_valid) {
        continue;
      }
      rm_vision_interfaces::msg::Armor armor_msg;
      armor_msg.header = msg->header;
      armor_msg.number = det.name;
      armor_msg.type = det.type;
      armor_msg.reproj_error = static_cast<float>(det.reproj_error);
      armor_msg.yaw = static_cast<float>(det.yaw_deg);
      armor_msg.pitch = static_cast<float>(det.pitch_deg);
      armor_msg.distance = static_cast<float>(det.distance_mm);

      const cv::Point2f center(
        det.box.x + det.box.width * 0.5F, det.box.y + det.box.height * 0.5F);
      armor_msg.distance_to_image_center =
        static_cast<float>(std::hypot(center.x - image_cx, center.y - image_cy));

      // PnP 结果：mm -> m；rvec -> 四元数
      armor_msg.pose.position.x = det.tvec[0] / 1000.0;
      armor_msg.pose.position.y = det.tvec[1] / 1000.0;
      armor_msg.pose.position.z = det.tvec[2] / 1000.0;

      cv::Mat rotation;
      cv::Rodrigues(cv::Mat(det.rvec), rotation);
      const tf2::Matrix3x3 tf2_rotation(
        rotation.at<double>(0, 0), rotation.at<double>(0, 1), rotation.at<double>(0, 2),
        rotation.at<double>(1, 0), rotation.at<double>(1, 1), rotation.at<double>(1, 2),
        rotation.at<double>(2, 0), rotation.at<double>(2, 1), rotation.at<double>(2, 2));
      tf2::Quaternion quaternion;
      tf2_rotation.getRotation(quaternion);
      armor_msg.pose.orientation = tf2::toMsg(quaternion);

      armors_msg.armors.push_back(armor_msg);
    }

    armors_pub_->publish(armors_msg);

    if (debug_ && debug_pub_.getNumSubscribers() > 0) {
      cv::Mat debug_image = drawDetections(cv_ptr->image, detections);
      cv_bridge::CvImage debug_msg;
      debug_msg.header = msg->header;
      debug_msg.encoding = sensor_msgs::image_encodings::BGR8;
      debug_msg.image = debug_image;
      debug_pub_.publish(*debug_msg.toImageMsg());
    }
  }

  static cv::Mat drawDetections(
    const cv::Mat & image, const std::vector<nr_vision_detector::ArmorDetection> & detections)
  {
    cv::Mat canvas = image.clone();
    for (const auto & det : detections) {
      std::vector<cv::Point> polygon;
      polygon.reserve(4);
      for (const auto & corner : det.corners) {
        polygon.emplace_back(
          static_cast<int>(std::lround(corner.x)), static_cast<int>(std::lround(corner.y)));
      }
      const cv::Scalar color = det.pose_valid ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 200, 255);
      cv::polylines(canvas, polygon, true, color, 2, cv::LINE_AA);
      const std::string text = det.label + " " + cv::format("%.1fm", det.distance_mm / 1000.0);
      cv::putText(
        canvas, text, cv::Point(polygon[0].x, std::max(15, polygon[0].y - 5)),
        cv::FONT_HERSHEY_SIMPLEX, 0.45, color, 1, cv::LINE_AA);
    }
    return canvas;
  }

  std::unique_ptr<nr_vision_detector::YoloDetector> detector_;
  cv::Mat camera_matrix_;
  cv::Mat dist_coeffs_;
  bool calib_logged_{false};
  bool debug_{true};

  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
  rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr info_sub_;
  rclcpp::Publisher<rm_vision_interfaces::msg::Armors>::SharedPtr armors_pub_;
  image_transport::Publisher debug_pub_;
};

}  // namespace rm_vision_cpp

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<rm_vision_cpp::DetectorNode>();
    rclcpp::spin(node);
  } catch (const std::exception & e) {
    std::fprintf(stderr, "armor_detector 启动失败: %s\n", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
