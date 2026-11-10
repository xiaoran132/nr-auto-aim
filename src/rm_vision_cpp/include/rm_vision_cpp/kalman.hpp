#ifndef RM_VISION_CPP__KALMAN_HPP_
#define RM_VISION_CPP__KALMAN_HPP_

#include <Eigen/Dense>

namespace rm_vision_cpp
{

/// 三维匀速模型卡尔曼滤波
/// 状态: [x, y, z, vx, vy, vz]
/// 观测: [x, y, z]
class ConstantVelocityKF
{
public:
  explicit ConstantVelocityKF(double process_noise = 1e-2, double measurement_noise = 1e-1);

  void reset(const Eigen::Vector3d & position);
  Eigen::Vector3d predict(double dt);
  Eigen::Vector3d update(const Eigen::Vector3d & position);

  bool initialized() const {return initialized_;}
  Eigen::Vector3d position() const {return x_.head<3>();}
  Eigen::Vector3d velocity() const {return x_.tail<3>();}

private:
  Eigen::Matrix<double, 6, 1> x_;
  Eigen::Matrix<double, 6, 6> P_;
  Eigen::Matrix<double, 6, 6> F_;
  Eigen::Matrix<double, 6, 6> Q_;
  Eigen::Matrix<double, 3, 6> H_;
  Eigen::Matrix3d R_;
  bool initialized_{false};
};

/// 把角度归一化到 [-pi, pi]
double wrapAngle(double angle);

}  // namespace rm_vision_cpp

#endif  // RM_VISION_CPP__KALMAN_HPP_
