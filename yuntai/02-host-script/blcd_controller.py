# -*- coding: utf-8 -*-
"""
MiniBalance 无刷电机驱动器 —— 串口上位机核心库

协议（逆向自 BLDC-Control.exe）：
    帧格式:  0x7A | 地址 | 命令码 | [数据(大端序)...] | XOR校验 | 0x7B
       - 地址(第2字节): 目标设备地址。出厂默认地址=2；可用命令 0x0D 修改设备地址。
                         读取参数命令 0x0C 用 0xFF 广播。
       - 校验: 从 0x7A 帧头起、到校验字节之前的所有字节逐字节异或，取低字节。
       - 例: 读取参数帧 = 7A FF 0C 89 7B （校验 = 0x7A ^ 0xFF ^ 0x0C = 0x89）

依赖: pyserial
"""

import struct
import time

import serial
import serial.tools.list_ports


class BLDCController:
    """MiniBalance 无刷电机驱动器控制器。

    每个命令帧都携带目标设备地址（frame 第 2 字节），用于区分同一总线上
    的多台设备。出厂默认地址=2，可用 set_address()（命令 0x0D）给某台改成 1。
    """

    # ---- 命令码（帧的第 3 字节）----
    # 映射依据：setupUi 反汇编中 setObjectName 对象名 → ui 偏移 → 按钮标签，
    #   与 qt_static_metacall 跳转表（槽名 → 命令码）交叉确认。
    #   对象名并非按序号顺序：ui+0x50=pushButton_7(使能), 0x54=pushButton_4(失能),
    #   0x58=pushButton_3(保存), 0x5C=pushButton_6(恢复出厂), pushButton_5(角度置零)。
    CMD_MODE        = 0x00   # 控制模式选择（数据 8bit）
    CMD_SPEED       = 0x01   # 速度控制（16 位有符号，单位 RPM）
    CMD_POSITION    = 0x02   # 多圈绝对角度（32 位有符号，×10）
    CMD_ANGLE       = 0x03   # 单圈绝对角度（16 位，0~359.9，×10）
    CMD_04          = 0x04   # （保留，官方手册未列出，勿用）
    CMD_DISABLE     = 0x05   # 电机失能（无数据）
    CMD_ENABLE      = 0x06   # 电机使能（无数据）
    CMD_ACCEL       = 0x07   # 加速度控制（16 位有符号，单位 转/s²）
    CMD_SAVE        = 0x08   # 保存设备参数（无数据，掉电保存）
    CMD_TURNS_CLR   = 0x09   # 电机上电后转过角度清零（无数据）
    CMD_ANGLE_ZERO  = 0x0A   # 单圈绝对角度设零点（无数据）
    CMD_FACTORY_RESET = 0x0B  # 恢复出厂设置（无数据，地址会重置回默认 2）
    CMD_READ_ALL    = 0x0C   # 读取全部参数（地址用 0xFF 广播）
    CMD_SET_ADDR    = 0x0D   # 地址设置（数据 1 字节：新地址）
    CMD_FEEDBACK    = 0x0E   # 数据反馈请求（数据 1 字节：类型 00~04）
    CMD_PID_KP_S    = 0x0F   # 速度环 KP（8 位，7 字节帧）
    CMD_PID_KI_S    = 0x10   # 速度环 KI（8 位，7 字节帧）
    CMD_PID_KP_P    = 0x11   # 位置环 KP（8 位，7 字节帧）
    CMD_PID_KI_P    = 0x12   # 位置环 KI（8 位，7 字节帧）

    # 控制模式（命令 0x00 的数据位；官方手册/反汇编确认）
    MODE_SPEED            = 0  # 速度控制
    MODE_MULTI_TURN_T     = 1  # 多圈位置控制-T（梯形加减速）
    MODE_SINGLE_TURN_T    = 2  # 单圈绝对位置控制-T
    MODE_MULTI_TURN_DIRECT = 3  # 多圈位置控制-直通
    MODE_SINGLE_TURN_DIRECT = 4  # 单圈绝对位置控制-直通

    ADDR_BROADCAST = 0xFF   # 广播地址（读取参数时用）

    FRAME_HEAD = 0x7A
    FRAME_TAIL = 0x7B

    def __init__(self, address=1):
        self.ser = None
        # 目标设备地址。出厂默认=2；云台约定 X(Yaw)=1、Y(Pitch)=2。
        # 也可在每个方法上用 addr= 参数临时覆盖。
        self.address = address

    # ------------------------------------------------------------------ #
    # 串口管理
    # ------------------------------------------------------------------ #
    @staticmethod
    def list_ports():
        """列出当前可用的串口设备名（如 COM3）。"""
        return [p.device for p in serial.tools.list_ports.comports()]

    def open(self, port, baudrate=115200, timeout=0.1):
        """
        打开串口。
        默认 115200 波特率 / 8 数据位 / 无校验 / 1 停止位 / 无流控
        （实测此板默认 115200，非文档所述 921600）；也可按需传入其它参数。
        """
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=timeout,
        )
        return self.ser.is_open

    def close(self):
        """关闭串口。"""
        if self.ser is not None:
            self.ser.close()
            self.ser = None

    # ------------------------------------------------------------------ #
    # 底层协议
    # ------------------------------------------------------------------ #
    @staticmethod
    def _checksum(data):
        """XOR 校验：对 data 的所有字节逐字节异或，返回低字节。"""
        c = 0
        for b in data:
            c ^= b
        return c & 0xFF

    def _frame(self, cmd, payload=b"", addr=None):
        """
        构造一帧：7A | addr | cmd | payload(大端, 可选) | checksum | 7B

        cmd     = 命令码（第 3 字节）
        payload = 数据（大端序已由调用方用 struct.pack(">...") 打好）
        addr    = 目标设备地址（第 2 字节）；None 时用 self.address
        """
        if addr is None:
            addr = self.address
        body = bytes((self.FRAME_HEAD, addr & 0xFF, cmd & 0xFF)) + bytes(payload)
        return body + bytes((self._checksum(body), self.FRAME_TAIL))

    def _send(self, frame):
        """发送一帧。"""
        if self.ser is None or not self.ser.is_open:
            raise RuntimeError("串口未打开，请先调用 open()")
        self.ser.reset_input_buffer()   # 丢弃残留数据，避免解析到陈旧帧
        self.ser.write(frame)
        self.ser.flush()

    def _read_frame(self, timeout=1.0, strict=True):
        """
        从串口读取一帧（0x7A ... 0x7B）。
        strict=True  : 校验失败则丢弃，直到得到一帧校验通过的数据。
        strict=False : 找到帧头/帧尾即返回（用于本板响应 checksum 常对不上的场景）。
        超时返回 None。
        """
        buf = bytearray()
        self.ser.timeout = 0.1
        deadline = time.time() + timeout
        while time.time() < deadline:
            b = self.ser.read(1)
            if b:
                buf.append(b[0])
            while True:
                start = buf.find(self.FRAME_HEAD)
                if start < 0:
                    buf.clear()
                    break
                end = buf.find(self.FRAME_TAIL, start + 1)
                if end < 0:
                    if start > 0:
                        del buf[:start]
                    break
                frame = bytes(buf[start:end + 1])
                del buf[:end + 1]
                if len(frame) < 4:
                    continue
                if not strict:
                    return frame
                if self._checksum(frame[:-2]) == frame[-2]:
                    return frame
        return None

    # ------------------------------------------------------------------ #
    # 高级命令
    # ------------------------------------------------------------------ #
    def read_params(self):
        """
        读取全部参数（命令 0x0C，地址 0xFF 广播），返回字典。
        响应帧 20 字节：7A | 地址回显 | 转过角度(32位,÷10) | 机械角度(16位,÷10)
            | 母线电压(16位,÷10) | 电机速度(16位,÷100) | 电流(16位,mA) | PID×4(各8位) | cs | 7B
        """
        # 本板响应常有位噪声（checksum 常对不上），故先严格、后宽松，多次重试。
        for strict in (True, False, False, False):
            self._send(self._frame(self.CMD_READ_ALL, addr=self.ADDR_BROADCAST))
            frame = self._read_frame(timeout=1.0, strict=strict)
            if frame is not None and len(frame) == 20 and frame[0] == 0x7A and frame[-1] == 0x7B:
                return self._parse_read_all(frame)
        raise TimeoutError("读取参数超时或未收到有效响应")

    def _parse_read_all(self, frame):
        """解析 0x0C 命令的 20 字节响应。

        实测帧样例（115200 波特率）：
            7A 00 | 00 00 00 0B | 02 E3 | 00 6E | 04 18 | 00 64 | 0A 09 32 05 | cs | 7B
        对应物理值：转过角度=1.1, 机械角度=73.9°, 母线电压=11.0V, 速度=10.48, 电流=100mA。
        注：帧第 2 字节（样例中 0x00）为设备地址回显，上位机会回写进「地址」输入框。
        """
        if len(frame) != 20:
            raise ValueError(f"响应帧长度异常: {len(frame)} 字节（应为 20）")
        data = frame[2:18]  # 16 字节数据区（跳过 0x7A 与地址回显字节）
        turns = struct.unpack(">i", data[0:4])[0]        # 转过角度 32位大端 ÷10
        angle = struct.unpack(">h", data[4:6])[0]        # 机械角度 16位大端 ÷10
        bus_voltage = struct.unpack(">h", data[6:8])[0]  # 母线电压 16位大端 ÷10
        speed = struct.unpack(">h", data[8:10])[0]       # 电机速度 16位大端 ÷100
        current = struct.unpack(">h", data[10:12])[0]    # 电流 16位大端 (mA)
        kp_s, ki_s, kp_p, ki_p = data[12], data[13], data[14], data[15]  # PID×4
        return {
            "turns":       turns,        # 转过角度（原始值，÷10 后为圈数）
            "angle":       angle,        # 机械角度（原始值，÷10 后为度）
            "bus_voltage": bus_voltage,  # 母线电压（原始值，÷10 后为伏）
            "speed":       speed,        # 电机速度（原始值，÷100）
            "current":     current,      # 电流（mA）
            "address":     frame[1],     # 设备地址回显（第 2 字节）
            "pid_speed_kp": kp_s,
            "pid_speed_ki": ki_s,
            "pid_pos_kp":   kp_p,
            "pid_pos_ki":   ki_p,
        }

    # ---- 设置命令（addr=None 时用 self.address；可传 addr 覆盖目标地址）----
    def set_speed(self, speed, addr=None):
        """设置电机速度（16 位有符号）。"""
        self._send(self._frame(self.CMD_SPEED, struct.pack(">h", speed), addr))

    def set_position(self, position, addr=None):
        """设置多圈绝对位置（命令 0x02，32 位有符号，单位 0.1°）。

        position 单位「度」，可跨多圈（如 720.0 = 两圈）；内部 ×10 取整后发送。
        """
        self._send(self._frame(self.CMD_POSITION, struct.pack(">i", int(round(position * 10))), addr))

    def set_angle(self, angle, addr=None):
        """设置单圈绝对角度（命令 0x03，16 位，范围 0~359.9°，单位 0.1°）。

        angle 单位「度」，内部 ×10 取整后发送。
        """
        self._send(self._frame(self.CMD_ANGLE, struct.pack(">h", int(round(angle * 10))), addr))

    def set_accel(self, accel, addr=None):
        """设置加速度（16 位有符号）。"""
        self._send(self._frame(self.CMD_ACCEL, struct.pack(">h", accel), addr))

    def set_address(self, new_addr, addr=None):
        """设置设备地址（命令 0x0D，数据 1 字节 = 新地址）。

        例：把当前地址 2 的电机改成地址 1 → set_address(1, addr=2)，帧 7A 02 0D 01 74 7B。
        改完需再调用 save_params() 掉电保存（保存帧要发到「新地址」）。
        注意：两台电机级联时，必须先拔掉其中一台再改，否则两台会同时被改成同一地址。
        """
        self._send(self._frame(self.CMD_SET_ADDR, bytes((new_addr & 0xFF,)), addr))

    def set_mode(self, mode, addr=None):
        """设置控制模式：7A [addr] 00 00 [mode] cs 7B（命令码 0x00，mode 为数据）。

        mode 取值见 MODE_* 常量：0=速度、1=多圈-T、2=单圈-T、3=多圈直通、4=单圈直通。
        """
        self._send(self._frame(self.CMD_MODE, bytes((0x00, mode & 0xFF)), addr))

    def set_pid_speed(self, kp, ki, addr=None):
        """设置速度环 PID（KP=0x0F, KI=0x10，各 8 位）。"""
        self._send(self._frame(self.CMD_PID_KP_S, bytes((0x00, kp & 0xFF)), addr))
        self._send(self._frame(self.CMD_PID_KI_S, bytes((0x00, ki & 0xFF)), addr))

    def set_pid_position(self, kp, ki, addr=None):
        """设置位置环 PID（KP=0x11, KI=0x12，各 8 位）。"""
        self._send(self._frame(self.CMD_PID_KP_P, bytes((0x00, kp & 0xFF)), addr))
        self._send(self._frame(self.CMD_PID_KI_P, bytes((0x00, ki & 0xFF)), addr))

    def angle_zero(self, addr=None):
        """当前机械角度设置为 0 点（命令 0x0A，无数据）。"""
        self._send(self._frame(self.CMD_ANGLE_ZERO, addr=addr))

    def enable(self, addr=None):
        """电机使能（命令 0x06，无数据）。"""
        self._send(self._frame(self.CMD_ENABLE, addr=addr))

    def disable(self, addr=None):
        """电机失能（命令 0x05，无数据）。"""
        self._send(self._frame(self.CMD_DISABLE, addr=addr))

    def spin(self, speed, addr=None, accel=100, delay=0.1):
        """一键让电机以给定转速旋转（速度控制模式）。

        完整序列：加速度(0x07) → 速度控制模式(0x00, mode=0) → 转速(0x01) → 使能(0x06)。
        speed = 目标转速（16 位有符号，如 50 ≈ 50 RPM；负值反转方向）。
        accel = 加速度（16 位有符号，单位 转/s²，默认 100，官方示例值）。
        delay = 各命令之间的间隔秒数（默认 0.1）。

        命令码以官方手册为准：0x01=转速、0x07=加速度（早期曾把两者弄反，
        导致用 0x04 当转速命令发出去后电机不转）。
        """
        self.set_accel(accel, addr=addr);  time.sleep(delay)
        self.set_mode(0, addr=addr);       time.sleep(delay)
        self.set_speed(speed, addr=addr);  time.sleep(delay)
        self.enable(addr=addr)

    def spin_cw(self, speed, addr=None, accel=100, delay=0.1):
        """顺时针（正向）旋转。内部就是 spin(+|speed|)。

        实际转向与电机接线有关：若你观察到的方向与此相反，请改用 spin_ccw。
        """
        self.spin(abs(speed), addr=addr, accel=accel, delay=delay)

    def spin_ccw(self, speed, addr=None, accel=100, delay=0.1):
        """逆时针（反向）旋转。内部就是 spin(-|speed|)。

        实际转向与电机接线有关：若方向相反，请改用 spin_cw。
        """
        self.spin(-abs(speed), addr=addr, accel=accel, delay=delay)

    def stop(self, addr=None, delay=0.1):
        """停转并失能：转速置 0 → 电机失能（0x05）。"""
        self.set_speed(0, addr=addr);  time.sleep(delay)
        self.disable(addr=addr)

    def move_position(self, position, addr=None, accel=100, mode=None, delay=0.1):
        """多圈位置控制：加速度(0x07) → 控制模式(0x00) → 目标位置(0x02) → 使能(0x06)。

        position 单位「度」，可跨多圈（如 720.0 = 两圈）；mode 默认 MODE_MULTI_TURN_T。
        """
        if mode is None:
            mode = self.MODE_MULTI_TURN_T
        self.set_accel(accel, addr=addr);  time.sleep(delay)
        self.set_mode(mode, addr=addr);    time.sleep(delay)
        self.set_position(position, addr=addr); time.sleep(delay)
        self.enable(addr=addr)

    def move_angle(self, angle, addr=None, accel=100, mode=None, delay=0.1):
        """单圈绝对位置控制：加速度(0x07) → 控制模式(0x00) → 目标角度(0x03) → 使能(0x06)。

        angle 单位「度」（0~359.9）；mode 默认 MODE_SINGLE_TURN_T。
        """
        if mode is None:
            mode = self.MODE_SINGLE_TURN_T
        self.set_accel(accel, addr=addr);  time.sleep(delay)
        self.set_mode(mode, addr=addr);    time.sleep(delay)
        self.set_angle(angle, addr=addr);  time.sleep(delay)
        self.enable(addr=addr)

    def save_params(self, addr=None):
        """保存设备参数到 EEPROM/Flash（命令 0x08，无数据）。"""
        self._send(self._frame(self.CMD_SAVE, addr=addr))

    def factory_reset(self, addr=None):
        """恢复出厂设置（命令 0x0B，无数据）。"""
        self._send(self._frame(self.CMD_FACTORY_RESET, addr=addr))

    # ---- 通用/待定参数命令（含义待定，供调试用）----
    def send_8bit(self, cmd, value, addr=None):
        """发送 8 位值命令（6 字节帧：7A addr cmd value cs 7B，如电流 0x0D）。"""
        self._send(self._frame(cmd, bytes((value & 0xFF,)), addr))

    def send_16bit(self, cmd, value, addr=None):
        """发送 16 位值命令（7 字节帧：7A addr cmd hi lo cs 7B）。"""
        self._send(self._frame(cmd, struct.pack(">h", value), addr))

    def send_pid8(self, cmd, value, addr=None):
        """发送 PID 类 8 位命令（7 字节帧：7A addr cmd 00 value cs 7B）。"""
        self._send(self._frame(cmd, bytes((0x00, value & 0xFF)), addr))
