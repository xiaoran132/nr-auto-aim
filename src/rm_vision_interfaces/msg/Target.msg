# 跟踪器输出的整车目标状态（惯性系下）
std_msgs/Header header

bool tracking
string id
int32 armors_num

# 整车中心与速度
geometry_msgs/Point position
geometry_msgs/Vector3 velocity

# 旋转状态（用于反陀螺）
float64 yaw
float64 v_yaw
float64 radius_1
float64 radius_2
float64 dz
