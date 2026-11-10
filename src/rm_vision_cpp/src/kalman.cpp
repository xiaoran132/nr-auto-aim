#include "rm_vision_cpp/kalman.hpp"

#include <algorithm>
#include <cmath>

namespace rm_vision_cpp
{

ConstantVelocityKF::ConstantVelocityKF(double process_noise, double measurement_noise)
{
  x_.setZero();
  P_ = Eigen::Matrix<double, 6, 6>::Identity() * 10.0;
  F_ = Eigen::Matrix<double, 6, 6>::Identity();
  Q_ = Eigen::Matrix<double, 6, 6>::Identity() * process_noise;
  H_.setZero();
  H_(0, 0) = 1.0;
  H_(1, 1) = 1.0;
  H_(2, 2) = 1.0;
  R_ = Eigen::Matrix3d::Identity() * measurement_noise;
}

//用观测位置重新初始化
void ConstantVelocityKF::reset(const Eigen::Vector3d & position)
{
  x_.setZero();
  x_.head<3>() = position;
  P_ = Eigen::Matrix<double, 6, 6>::Identity() * 10.0;
  initialized_ = true;
}

//dt 外推状态
Eigen::Vector3d ConstantVelocityKF::predict(double dt)
{
  const double step = std::max(dt, 1e-3);
  F_.setIdentity();
  F_.block<3, 3>(0, 3) = Eigen::Matrix3d::Identity() * step;

  x_ = F_ * x_;
  P_ = F_ * P_ * F_.transpose() + Q_ * step;
  return x_.head<3>();
}

//用观测位置修正状态
Eigen::Vector3d ConstantVelocityKF::update(const Eigen::Vector3d & position)
{
  if (!initialized_) {
    reset(position);
    return x_.head<3>();
  }

  const Eigen::Vector3d innovation = position - H_ * x_;
  const Eigen::Matrix3d S = H_ * P_ * H_.transpose() + R_;
  const Eigen::Matrix<double, 6, 3> K = P_ * H_.transpose() * S.inverse();

  x_ = x_ + K * innovation;
  P_ = (Eigen::Matrix<double, 6, 6>::Identity() - K * H_) * P_;
  return x_.head<3>();
}

//角度归一化
double wrapAngle(double angle)
{
  while (angle > M_PI) {
    angle -= 2.0 * M_PI;
  }
  while (angle < -M_PI) {
    angle += 2.0 * M_PI;
  }
  return angle;
}

}  // namespace rm_vision_cpp
