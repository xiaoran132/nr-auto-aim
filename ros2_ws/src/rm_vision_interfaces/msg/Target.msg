# 跟踪器输出的目标状态
std_msgs/Header header

bool tracking
string id
int32 armors_num

# 位置与速度（与 header.frame_id 同坐标系；默认相机光学系，配置 target_frame 后为 TF 目标系）
geometry_msgs/Point position
geometry_msgs/Vector3 velocity

# 旋转状态（用于反陀螺）
float64 yaw
float64 v_yaw
float64 radius_1
float64 radius_2
float64 dz
