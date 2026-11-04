#include <chrono>
#include <cmath>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include "rm_vision_cpp/kalman.hpp"
#include "rm_vision_interfaces/msg/armors.hpp"
#include "rm_vision_interfaces/msg/target.hpp"

namespace rm_vision_cpp
{

class ArmorTrackerNode : public rclcpp::Node
{
public:
  ArmorTrackerNode()
  : Node("armor_tracker")
  {
    declare_parameter("lost_time_thres", 1.0);
    declare_parameter("process_noise", 0.01);
    declare_parameter("measurement_noise", 0.1);

    lost_time_thres_ = get_parameter("lost_time_thres").as_double();
    kf_ = ConstantVelocityKF(
      get_parameter("process_noise").as_double(),
      get_parameter("measurement_noise").as_double());

    armors_sub_ = create_subscription<rm_vision_interfaces::msg::Armors>(
      "armors", 10, std::bind(&ArmorTrackerNode::onArmors, this, std::placeholders::_1));
    target_pub_ = create_publisher<rm_vision_interfaces::msg::Target>("target", 10);
    timer_ = create_wall_timer(
      std::chrono::milliseconds(50), std::bind(&ArmorTrackerNode::onTimer, this));

    RCLCPP_INFO(get_logger(), "跟踪节点启动");
  }

private:
  void onArmors(const rm_vision_interfaces::msg::Armors::SharedPtr msg)
  {
    if (msg->armors.empty()) {
      return;
    }

    // 选距离图像中心最近的一块
    const auto * best = &msg->armors.front();
    for (const auto & armor : msg->armors) {
      if (armor.distance_to_image_center < best->distance_to_image_center) {
        best = &armor;
      }
    }

    const double now_sec = now().seconds();
    if (last_stamp_sec_ >= 0.0) {
      const double dt = now_sec - last_stamp_sec_;
      if (dt > 0.0) {
        kf_.predict(dt);
      }
    }

    const Eigen::Vector3d position(
      best->pose.position.x, best->pose.position.y, best->pose.position.z);
    kf_.update(position);

    const double measured_yaw = best->yaw * M_PI / 180.0;
    yaw_ = wrapAngle(yaw_ + 0.5 * wrapAngle(measured_yaw - yaw_));

    last_stamp_sec_ = now_sec;
    last_seen_sec_ = now_sec;
    tracking_ = true;
  }

  void onTimer()
  {
    if (last_seen_sec_ < 0.0) {
      return;
    }

    if (now().seconds() - last_seen_sec_ > lost_time_thres_) {
      tracking_ = false;
    }

    rm_vision_interfaces::msg::Target target;
    target.header.stamp = now();
    target.header.frame_id = "camera_optical_frame";
    target.tracking = tracking_;
    target.id = "target_0";
    target.armors_num = 4;

    const Eigen::Vector3d position = kf_.position();
    const Eigen::Vector3d velocity = kf_.velocity();
    target.position.x = position.x();
    target.position.y = position.y();
    target.position.z = position.z();
    target.velocity.x = velocity.x();
    target.velocity.y = velocity.y();
    target.velocity.z = velocity.z();
    target.yaw = yaw_;
    target.v_yaw = 0.0;
    target.radius_1 = 0.0;
    target.radius_2 = 0.0;
    target.dz = 0.0;

    target_pub_->publish(target);
  }

  ConstantVelocityKF kf_;
  double lost_time_thres_{1.0};
  bool tracking_{false};
  double yaw_{0.0};
  double last_stamp_sec_{-1.0};
  double last_seen_sec_{-1.0};

  rclcpp::Subscription<rm_vision_interfaces::msg::Armors>::SharedPtr armors_sub_;
  rclcpp::Publisher<rm_vision_interfaces::msg::Target>::SharedPtr target_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace rm_vision_cpp

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rm_vision_cpp::ArmorTrackerNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
