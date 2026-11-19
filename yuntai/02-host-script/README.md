# MiniBalance 无刷电机驱动器 —— Python 上位机

这是根据逆向分析 `BLDC-Control.exe`（MiniBalance 无刷电机驱动器官方上位机）
还原出的**独立 Python 串口上位机**，仅依赖 `pyserial`。

## 依赖安装

```bash
pip install -r requirements.txt
```

## 串口协议（逆向还原）

### 帧格式

```
0x7A | 地址 | 命令码 | [数据(大端序)...] | XOR校验 | 0x7B
```

- **地址（第 2 字节）**：目标设备地址。双电机云台中 `1`=下电机、`2`=上电机；
  读取参数命令用 `0xFF` 广播。
- **命令码（第 3 字节）**：见下表。
- **校验**：从 `0x7A` 帧头起、到校验字节之前的所有字节逐字节异或，取低字节。
- 例：读取参数帧 `7A FF 0C 89 7B`（校验 `= 0x7A ^ 0xFF ^ 0x0C = 0x89`）。

### 命令表

> 所有命令帧的第 2 字节都是目标地址 `[addr]`（1=下电机，2=上电机）。

| 命令 | 功能 | 帧 | 数据 |
|------|------|-----|------|
| `0x00` | 控制模式选择 | `7A [addr] 00 00 [mode] cs 7B` | 8 位（模式索引） |
| `0x01` | 加速度设置 | `7A [addr] 01 [h][l] cs 7B` | 16 位有符号 |
| `0x02` | 位置设置（多圈） | `7A [addr] 02 [b3][b2][b1][b0] cs 7B` | 32 位有符号（圈数含在内） |
| `0x03` | 角度设置（单圈） | `7A [addr] 03 [h][l] cs 7B` | 16 位 |
| `0x04` | 速度设置 | `7A [addr] 04 [h][l] cs 7B` | 16 位有符号 |
| `0x05` | 地址设置 | `7A [新地址] 05 cs 7B` | 8 位（地址字节即新地址） |
| `0x06` | 参数（待定，疑恢复出厂） | `7A [addr] 06 cs 7B` | 无 |
| `0x07` | 参数（待定） | `7A [addr] 07 [h][l] cs 7B` | 16 位 |
| `0x08` | 保存参数 | `7A [addr] 08 cs 7B` | 无 |
| `0x0A` | 电机使能（推测） | `7A [addr] 0A cs 7B` | 无 |
| `0x0B` | 电机失能（推测） | `7A [addr] 0B cs 7B` | 无 |
| `0x0C` | 读取全部参数 | `7A FF 0C cs 7B`（FF=广播） | 无 |
| `0x0D` | 电流设置 | `7A [addr] 0D [v] cs 7B` | 8 位（6 字节帧） |
| `0x0F` | 速度环 PID KP | `7A [addr] 0F 00 [v] cs 7B` | 8 位 |
| `0x10` | 速度环 PID KI | `7A [addr] 10 00 [v] cs 7B` | 8 位 |
| `0x11` | 位置环 PID KP | `7A [addr] 11 00 [v] cs 7B` | 8 位 |
| `0x12` | 位置环 PID KI | `7A [addr] 12 00 [v] cs 7B` | 8 位 |

### 读取全部参数（`0x0C`）响应：20 字节

```
7A | 地址回显 | 转过角度(32位) | 机械角度(16位) | 母线电压(16位) | 速度(16位) | 电流(16位) | PID×4(各8位) | cs | 7B
```

字段单位：转过角度÷10、机械角度÷10、母线电压÷10、速度÷100、电流单位 mA。

## 用法

### 命令行

```bash
python cli.py list                                # 列出可用串口
python cli.py --port COM3 read                    # 读取全部参数（广播）
python cli.py --port COM3 --address 1 speed 500   # 设置下电机速度
python cli.py --port COM3 --address 2 speed 500   # 设置上电机速度
python cli.py --port COM3 position 1024           # 设置位置（32 位，含圈数）
python cli.py --port COM3 angle 90                # 设置角度
python cli.py --port COM3 accel 100               # 设置加速度
python cli.py --port COM3 current 10              # 设置电流
python cli.py --port COM3 address 1               # 设置设备地址
python cli.py --port COM3 enable                  # 电机使能
python cli.py --port COM3 disable                 # 电机失能
python cli.py --port COM3 save                    # 保存参数
python cli.py --port COM3 reset                   # 恢复出厂设置
python cli.py --port COM3 mode 1                  # 切换控制模式
```

### 作为库使用

```python
from blcd_controller import BLDCController

ctrl = BLDCController(address=1)     # 地址 1=下电机，2=上电机
ctrl.open("COM3")                    # 默认 115200 / 8N1

params = ctrl.read_params()          # 读取全部参数（广播，返回字典）
print(params["speed"], params["bus_voltage"])

ctrl.set_speed(500)                  # 设置速度
ctrl.enable()                        # 使能
ctrl.set_position(1024)              # 设置位置（32 位，含圈数）
ctrl.save_params()                   # 保存参数

ctrl.close()
```

## 注意事项

1. **波特率**：默认 `115200`（实测此板默认值）；如你的固件配置不同，可用
   `ctrl.open("COM3", baudrate=921600)` 或 CLI 的 `--baud` 覆盖。
2. **待定命令**：`0x07 / 0x0A / 0x0B / 0x0F / 0x10 / 0x11 / 0x12` 的确切含义
   尚未从二进制中确认，已提供 `send_8bit(cmd, val)` / `send_16bit(cmd, val)`
   通用方法供调试；`factory_reset()` 的 `0x0A` 命令码为推测。
3. **帧解析健壮性**：`_read_frame()` 用缓冲累积处理了粘包/半包，并校验 XOR，
   校验失败自动丢弃该帧继续寻找下一帧。
4. **字节序**：所有多字节数据均为**大端序**（网络字节序），用 `struct.pack(">h")`
   等编码。
