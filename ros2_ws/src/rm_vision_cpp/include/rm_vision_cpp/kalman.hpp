#pragma once

#include <Eigen/Dense>

namespace rm_vision_cpp
{
// 三维匀速模型卡尔曼滤波（沿用 ros2 分支实现）
// 状态: [x, y, z, vx, vy, vz]，观测: [x, y, z]
class ConstantVelocityKF
{
public:
  ConstantVelocityKF(double process_noise = 1e-2, double measurement_noise = 1e-1);

  void reset(const Eigen::Vector3d & position);
  Eigen::Vector3d predict(double dt);
  Eigen::Vector3d update(const Eigen::Vector3d & position);

  bool initialized() const { return initialized_; }
  Eigen::Vector3d position() const { return x_.head<3>(); }
  Eigen::Vector3d velocity() const { return x_.tail<3>(); }

private:
  Eigen::Matrix<double, 6, 1> x_;
  Eigen::Matrix<double, 6, 6> P_;
  Eigen::Matrix<double, 6, 6> F_;
  Eigen::Matrix<double, 6, 6> Q_;
  Eigen::Matrix<double, 3, 6> H_;
  Eigen::Matrix3d R_;
  bool initialized_{false};
};

double wrapAngle(double angle);
}  // namespace rm_vision_cpp
