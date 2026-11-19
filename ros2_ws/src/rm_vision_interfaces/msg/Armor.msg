# 单块装甲板的检测结果（相机光学坐标系下）
std_msgs/Header header

# 语义信息
string number                 # 兵种/编号：one..five / sentry / outpost / base ...
string type                   # "small" / "large"

# 图像层信息
float32 distance_to_image_center   # 像素距离，用于选板
float32 reproj_error               # PnP 重投影误差（px）

# 位姿与角度（相机光学系：x 右、y 下、z 前）
geometry_msgs/Pose pose       # 位姿：position 单位 m
float32 yaw                   # 度，右为正
float32 pitch                 # 度，上为正
float32 distance              # mm，目标到相机光心的距离
