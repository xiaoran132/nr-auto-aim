# -*- coding: utf-8 -*-
"""
树莓派控制 MiniBalance F32C 云台电机的示例脚本。

依赖：本目录下的 blcd_controller.py（需先 pip install pyserial）。

在树莓派上运行：
    python pi_demo.py /dev/ttyUSB0
（不传参数默认 /dev/ttyUSB0；用 `ls /dev/ttyUSB* /dev/ttyACM*` 确认设备名）
"""

import sys
import time

from blcd_controller import BLDCController


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"

    c = BLDCController()
    c.open(port, 115200)
    print(f"已打开串口 {port}")

    # —— 示例 1：X 轴（Yaw，地址 1）顺时针 50 RPM，转 3 秒后停 ——
    print("X 轴顺时针 50 RPM，3 秒")
    c.spin_cw(50, addr=1)
    time.sleep(3)
    c.stop(addr=1)

    time.sleep(1)

    # —— 示例 2：Y 轴（Pitch，地址 2）单圈绝对位置转到 90° ——
    print("Y 轴单圈位置控制到 90°")
    c.move_angle(90.0, addr=2)
    time.sleep(3)

    c.close()
    print("完成")


if __name__ == "__main__":
    main()
