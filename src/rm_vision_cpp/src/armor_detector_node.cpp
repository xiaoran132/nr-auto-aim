#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/calib3d.hpp>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.hpp>
#include <opencv2/core.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/image.hpp>

#include "rm_vision_cpp/armor_detect.hpp"
#include "rm_vision_interfaces/msg/armor.hpp"
#include "rm_vision_interfaces/msg/armors.hpp"

namespace rm_vision_cpp
{

class ArmorDetectorNode : public rclcpp::Node
{
public:
  ArmorDetectorNode()
  : Node("armor_detector")
  {
    declare_parameter("detect_color", "red");
    declare_parameter("debug", true);
    declare_parameter("min_distance_mm", 300.0);
    declare_parameter("max_distance_mm", 20000.0);

    DetectorParams params;
    params.color = get_parameter("detect_color").as_string();
    params.min_distance_mm = get_parameter("min_distance_mm").as_double();
    params.max_distance_mm = get_parameter("max_distance_mm").as_double();
    detector_ = std::make_unique<ArmorDetector>(params);
    debug_ = get_parameter("debug").as_bool();

    camera_matrix_ = (cv::Mat_<double>(3, 3) << 1408.0, 0.0, 640.0, 0.0, 1408.0, 512.0, 0.0, 0.0, 1.0);
    dist_coeffs_ = cv::Mat::zeros(5, 1, CV_64F);

    image_sub_ = create_subscription<sensor_msgs::msg::Image>(
      "image_raw", 10, std::bind(&ArmorDetectorNode::onImage, this, std::placeholders::_1));
    info_sub_ = create_subscription<sensor_msgs::msg::CameraInfo>(
      "camera_info", 10, std::bind(&ArmorDetectorNode::onCameraInfo, this, std::placeholders::_1));

    armors_pub_ = create_publisher<rm_vision_interfaces::msg::Armors>("armors", 10);
    if (debug_) {
      debug_pub_ = create_publisher<sensor_msgs::msg::Image>("~/debug_image", 10);
    }

    RCLCPP_INFO(get_logger(), "检测节点启动, 颜色=%s", params.color.c_str());
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
        camera_matrix_.at<double>(r, c) = msg->k[r * 3 + c];
      }
    }
    if (!msg->d.empty()) {
      dist_coeffs_ = cv::Mat(static_cast<int>(msg->d.size()), 1, CV_64F);
      for (size_t i = 0; i < msg->d.size(); ++i) {
        dist_coeffs_.at<double>(static_cast<int>(i), 0) = msg->d[i];
      }
    }
  }

  void onImage(const sensor_msgs::msg::Image::SharedPtr msg)
  {
    cv_bridge::CvImagePtr cv_ptr;
    try {
      cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
    } catch (const cv_bridge::Exception & ex) {
      RCLCPP_ERROR(get_logger(), "图像转换失败: %s", ex.what());
      return;
    }

    const auto armors = detector_->detect(cv_ptr->image, camera_matrix_, dist_coeffs_);

    rm_vision_interfaces::msg::Armors out;
    out.header = msg->header;
    const cv::Point2f center(cv_ptr->image.cols / 2.0F, cv_ptr->image.rows / 2.0F);

    for (const auto & armor : armors) {
      rm_vision_interfaces::msg::Armor item;
      item.header = msg->header;
      item.type = armor.type;
      item.number = "";

      cv::Point2f armor_center(0.0F, 0.0F);
      for (const auto & corner : armor.corners) {
        armor_center += corner;
      }
      armor_center *= 0.25F;
      item.distance_to_image_center = static_cast<float>(std::hypot(
          static_cast<double>(armor_center.x - center.x),
          static_cast<double>(armor_center.y - center.y)));
      item.reproj_error = static_cast<float>(armor.reproj_error);
      item.yaw = static_cast<float>(armor.yaw);
      item.pitch = static_cast<float>(armor.pitch);
      item.distance = static_cast<float>(armor.distance_mm);

      item.pose.position.x = armor.tvec[0] / 1000.0;
      item.pose.position.y = armor.tvec[1] / 1000.0;
      item.pose.position.z = armor.tvec[2] / 1000.0;

      cv::Mat rotation;
      cv::Rodrigues(cv::Mat(armor.rvec), rotation);
      const double trace = rotation.at<double>(0, 0) + rotation.at<double>(1, 1) +
        rotation.at<double>(2, 2);
      if (trace > 0.0) {
        const double s = std::sqrt(trace + 1.0) * 2.0;
        item.pose.orientation.w = 0.25 * s;
        item.pose.orientation.x = (rotation.at<double>(2, 1) - rotation.at<double>(1, 2)) / s;
        item.pose.orientation.y = (rotation.at<double>(0, 2) - rotation.at<double>(2, 0)) / s;
        item.pose.orientation.z = (rotation.at<double>(1, 0) - rotation.at<double>(0, 1)) / s;
      } else {
        item.pose.orientation.w = 1.0;
      }

      out.armors.push_back(item);
    }
    armors_pub_->publish(out);

    if (debug_ && debug_pub_) {
      cv_bridge::CvImage debug_msg;
      debug_msg.header = msg->header;
      debug_msg.encoding = sensor_msgs::image_encodings::BGR8;
      debug_msg.image = drawDetections(cv_ptr->image, armors);
      debug_pub_->publish(*debug_msg.toImageMsg());
    }
  }

  std::unique_ptr<ArmorDetector> detector_;
  bool debug_{true};
  cv::Mat camera_matrix_;
  cv::Mat dist_coeffs_;

  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
  rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr info_sub_;
  rclcpp::Publisher<rm_vision_interfaces::msg::Armors>::SharedPtr armors_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr debug_pub_;
};

}  // namespace rm_vision_cpp

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rm_vision_cpp::ArmorDetectorNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
