# BLDC-Control.exe 反汇编分析文档（任务 3）

本目录包含对 `D:\BLDC-Control\BLDC-Control.exe` 关键函数的反汇编注解与协议/结构分析。

目标程序：MiniBalance 无刷直流电机（BLDC）上位机，Qt5（Core/Gui/Widgets/SerialPort），
MinGW-w64 GCC 4.9.3/5.3.0 编译，32 位 PE（i686，dwarf 异常模型），剥离到外部 PDB，
但导入表（IAT）保留了完整的 Itanium-ABI C++ 符号，可经 `c++filt` 还原。

## 文件清单

| 文件 | 内容 |
|------|------|
| `annotated_disasm.asm` | 关键函数逐行反汇编 + 行内注释（构造函数、datareceived、checksum、refreshserial、connect 处理） |
| `constructor_analysis.md` | Widget 构造函数完整重建：成员初始化、setupUi、串口配置参数、全部 connect 调用、QTimer 用途 |
| `datareceived_analysis.md` | 接收解析逻辑：帧校验步骤、20 字节参数块字段提取（偏移+字节序）、每个字段写入的 QLineEdit |
| `protocol_notes.md` | 串口协议细节，与命令表交叉验证 |

## 关键结论速览

### 构造函数串口配置
- 波特率：**不在构造函数里设置**，在“连接”按钮点击时从下拉框读取后设置
- 数据位 `DataBits=8`、停止位 `OneStop`、校验 `NoParity`、流控 `NoFlowControl`

### 信号连接（2 条）
| 发送者 | 信号 | 槽 |
|--------|------|----|
| `serialPort` | `QIODevice::readyRead` | `Widget::datareceived` |
| `timer` | `QTimer::timeout` | `Widget::refreshserial` |

### 接收帧 20 字节字段映射
`7A | CMD | 6×u16(大端) | 4×u8 | XOR | 7B`，详见 `datareceived_analysis.md`。

### 校验和
XOR（异或）：从 `0x7A` 起（含）到最后一个数据字节（含）逐字节异或。
