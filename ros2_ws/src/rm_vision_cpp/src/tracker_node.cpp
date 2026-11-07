// 跟踪节点：选板 -> KF -> /target（可选 tf2 变换到 target_frame）
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <tf2/exceptions.h>
#include <tf2/time.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include "rm_vision_cpp/kalman.hpp"
#include "rm_vision_interfaces/msg/armors.hpp"
#include "rm_vision_interfaces/msg/target.hpp"

namespace rm_vision_cpp
{
class TrackerNode : public rclcpp::Node
{
public:
  TrackerNode()
  : Node("armor_tracker")
  {
    lost_time_thres_ = this->declare_parameter<double>("lost_time_thres", 0.5);
    const double process_noise = this->declare_parameter<double>("process_noise", 0.01);
    const double measurement_noise = this->declare_parameter<double>("measurement_noise", 0.1);
    publish_rate_ = this->declare_parameter<double>("publish_rate", 20.0);
    target_frame_ = this->declare_parameter<std::string>("target_frame", "");

    kf_ = ConstantVelocityKF(process_noise, measurement_noise);

    if (!target_frame_.empty()) {
      tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
      tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
      RCLCPP_INFO(
        this->get_logger(), "启用 tf2 变换：armor 位姿 -> %s", target_frame_.c_str());
    }

    rclcpp::QoS qos(rclcpp::KeepLast(10));
    qos = rclcpp::SensorDataQoS();
    armors_sub_ = this->create_subscription<rm_vision_interfaces::msg::Armors>(
      "armors", qos, std::bind(&TrackerNode::onArmors, this, std::placeholders::_1));
    target_pub_ = this->create_publisher<rm_vision_interfaces::msg::Target>("target", qos);

    const auto period = std::chrono::duration<double>(1.0 / std::max(publish_rate_, 1.0));
    timer_ = this->create_wall_timer(period, std::bind(&TrackerNode::onTimer, this));
  }

//收到检测结果时更新滤波器
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

    Eigen::Vector3d position(
      best->pose.position.x, best->pose.position.y, best->pose.position.z);
    std::string frame = best->header.frame_id;

    if (tf_buffer_ && !target_frame_.empty() && frame != target_frame_) {
      geometry_msgs::msg::PoseStamped in;
      in.header = best->header;
      in.pose = best->pose;
      try {
        const auto out = tf_buffer_->transform(in, target_frame_, tf2::durationFromSec(0.02));
        position = Eigen::Vector3d(out.pose.position.x, out.pose.position.y, out.pose.position.z);
        frame = target_frame_;
      } catch (const tf2::TransformException & ex) {
        RCLCPP_WARN_THROTTLE(
          this->get_logger(), *this->get_clock(), 2000,
          "tf2 变换失败（先发布 odom->camera 的 TF）：%s", ex.what());
      }
    }
    frame_ = frame;

    // 用图像 header.stamp 而不是 now()，避免把排队/传输延迟算进速度
    const rclcpp::Time stamp(msg->header.stamp);
    if (last_stamp_set_) {
      const double dt = (stamp - last_stamp_).seconds();
      if (dt > 0.0) {
        kf_.predict(dt);
      }
    }
    kf_.update(position);

     //低通平滑，yaw
    const double measured_yaw = static_cast<double>(best->yaw) * M_PI / 180.0;
    yaw_ = wrapAngle(yaw_ + 0.5 * wrapAngle(measured_yaw - yaw_));

    last_stamp_ = stamp;
    last_stamp_set_ = true;
    last_seen_ = stamp;
    tracking_ = true;
  }

//定时发布目标状态
  void onTimer()
  {
    if (!last_stamp_set_) {
      return;
    }
    if ((this->now() - last_seen_).seconds() > lost_time_thres_) {
      tracking_ = false;
    }
    
  //组装 Target 消息
    rm_vision_interfaces::msg::Target target;
    target.header.stamp = this->now();
    target.header.frame_id = frame_.empty() ? "camera_optical_frame" : frame_;
    target.tracking = tracking_;
    target.id = "target_0";
    target.armors_num = 4;

    if (tracking_) {
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
    }
    target_pub_->publish(target);
  }

//成员变量
  ConstantVelocityKF kf_{0.01, 0.1};
  double lost_time_thres_{0.5};
  double publish_rate_{20.0};
  std::string target_frame_;
  std::string frame_;
  bool tracking_{false};
  bool last_stamp_set_{false};
  double yaw_{0.0};
  rclcpp::Time last_stamp_{0, 0, RCL_ROS_TIME};
  rclcpp::Time last_seen_{0, 0, RCL_ROS_TIME};

  rclcpp::Subscription<rm_vision_interfaces::msg::Armors>::SharedPtr armors_sub_;
  rclcpp::Publisher<rm_vision_interfaces::msg::Target>::SharedPtr target_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
};

}  // namespace rm_vision_cpp

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<rm_vision_cpp::TrackerNode>();
    rclcpp::spin(node);
  } catch (const std::exception & e) {
    std::fprintf(stderr, "armor_tracker 启动失败: %s\n", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
