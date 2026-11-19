# -*- coding: utf-8 -*-
"""
MiniBalance 无刷电机驱动器 —— 命令行上位机

用法示例:
    python cli.py list                          # 列出可用串口
    python cli.py --port COM3 read              # 读取全部参数(广播)
    python cli.py --port COM3 --address 1 speed 500    # 设置转速（地址 1 = X/Yaw）
    python cli.py --port COM3 --address 2 speed 500    # 设置转速（地址 2 = Y/Pitch）
    python cli.py --port COM3 --address 1 position 720   # 多圈位置控制到 720°（两圈）
    python cli.py --port COM3 --address 1 angle 90       # 单圈绝对位置控制到 90°
    python cli.py --port COM3 accel 100         # 设置加速度(0x07)
    python cli.py --port COM3 setaddr 1         # 设置设备地址(0x0D)；--address 为当前地址
    python cli.py --port COM3 zero              # 当前机械角度置零
    python cli.py --port COM3 enable            # 电机使能
    python cli.py --port COM3 disable           # 电机失能
    python cli.py --port COM3 save              # 保存参数
    python cli.py --port COM3 reset             # 恢复出厂设置
    python cli.py --port COM3 mode 0            # 切换控制模式(0=速度,1=多圈T,2=单圈T,3=多圈直通,4=单圈直通)
    python cli.py --port COM3 spin 50           # 一键旋转: accel+速度模式+转速+使能
    python cli.py --port COM3 cw 50             # 顺时针旋转
    python cli.py --port COM3 ccw 50            # 逆时针旋转
    python cli.py --port COM3 stop              # 停转并失能
"""

import argparse
import sys

from blcd_controller import BLDCController


def cmd_list():
    ports = BLDCController.list_ports()
    if not ports:
        print("未发现串口设备")
        return
    print("可用串口:")
    for p in ports:
        print("  " + p)


def print_params(params):
    print("=" * 40)
    print("设备参数:")
    print(f"  设备地址   : {params.get('address', '?')}")
    print(f"  转过角度   : {params['turns'] / 10.0:.1f} (圈)")
    print(f"  机械角度   : {params['angle'] / 10.0:.1f} (°)")
    print(f"  母线电压   : {params['bus_voltage'] / 10.0:.1f} (V)")
    print(f"  电机速度   : {params['speed'] / 100.0:.2f}")
    print(f"  电流       : {params['current']} (mA)")
    print(f"  速度PID KP : {params['pid_speed_kp']}")
    print(f"  速度PID KI : {params['pid_speed_ki']}")
    print(f"  位置PID KP : {params['pid_pos_kp']}")
    print(f"  位置PID KI : {params['pid_pos_ki']}")
    print("=" * 40)


def main():
    parser = argparse.ArgumentParser(description="MiniBalance 无刷电机驱动器上位机")
    parser.add_argument("--port", help="串口设备名，如 COM3")
    parser.add_argument("--baud", type=int, default=115200, help="波特率（默认 115200）")
    parser.add_argument("--address", type=int, default=1,
                        help="目标设备地址（X/Yaw=1, Y/Pitch=2；出厂默认 2）")
    parser.add_argument("command", help="命令")
    parser.add_argument("args", nargs="*", help="命令参数")
    args = parser.parse_args()

    if args.command == "list":
        cmd_list()
        return

    if not args.port:
        parser.error("除 list 外均需指定 --port")

    ctrl = BLDCController(address=args.address)
    try:
        ctrl.open(args.port, baudrate=args.baud)
    except Exception as e:
        print(f"打开串口 {args.port} 失败: {e}")
        sys.exit(1)

    try:
        if args.command == "read":
            print_params(ctrl.read_params())
        elif args.command == "speed":
            ctrl.set_speed(int(args.args[0]))
            print(f"已设置速度 {args.args[0]}")
        elif args.command == "position":
            pos = float(args.args[-1])
            ctrl.move_position(pos)
            print(f"已执行多圈位置控制，目标 {pos}°（accel+模式+位置+使能）")
        elif args.command == "angle":
            ang = float(args.args[0])
            ctrl.move_angle(ang)
            print(f"已执行单圈绝对位置控制，目标 {ang}°（accel+模式+角度+使能）")
        elif args.command == "accel":
            ctrl.set_accel(int(args.args[0]))
            print(f"已设置加速度 {args.args[0]}")
        elif args.command == "setaddr":
            new_addr = int(args.args[0])
            ctrl.set_address(new_addr)
            print(f"已发送地址设置：当前地址 {args.address} → 新地址 {new_addr}")
            print(f"接着执行掉电保存：python cli.py --port {args.port} --address {new_addr} save")
        elif args.command == "zero":
            ctrl.angle_zero()
            print("已发送角度置零命令")
        elif args.command == "enable":
            ctrl.enable()
            print("电机已使能")
        elif args.command == "disable":
            ctrl.disable()
            print("电机已失能")
        elif args.command == "save":
            ctrl.save_params()
            print("已发送保存参数命令")
        elif args.command == "reset":
            ctrl.factory_reset()
            print("已发送恢复出厂设置命令")
        elif args.command == "mode":
            ctrl.set_mode(int(args.args[0]))
            print(f"已切换控制模式 {args.args[0]}")
        elif args.command == "spin":
            ctrl.spin(int(args.args[0]))
            print(f"已启动旋转，转速 {args.args[0]}（accel=100 + 速度模式 + 使能）")
        elif args.command == "cw":
            ctrl.spin_cw(int(args.args[0]))
            print(f"已启动顺时针旋转，转速 {args.args[0]}（若方向反了请用 ccw）")
        elif args.command == "ccw":
            ctrl.spin_ccw(int(args.args[0]))
            print(f"已启动逆时针旋转，转速 {args.args[0]}（若方向反了请用 cw）")
        elif args.command == "stop":
            ctrl.stop()
            print("已停转并失能")
        else:
            print(f"未知命令: {args.command}")
            parser.print_help()
            sys.exit(1)
    except Exception as e:
        print(f"命令执行失败: {e}")
        sys.exit(1)
    finally:
        ctrl.close()


if __name__ == "__main__":
    main()
