# BLDC-Control.exe 反编译伪代码

本文档是对 `D:\BLDC-Control\BLDC-Control.exe`（173056 字节，PE32 i386）的可读化反编译结果。

- **应用身份**：MiniBalance 无刷电机（BLDC）驱动器上位机（`https://minibalance.taobao.com/`）
- **框架**：Qt 5.5（Qt5Core / Qt5Gui / Qt5Widgets / Qt5SerialPort）
- **编译器**：MinGW-w64 GCC 4.9.3 / 5.3.0（i686-posix-dwarf，32 位）
- **窗口标题**：`BLDC-Control`，类名 `Widget`，主窗口 `frmMain`

---

## 1. 类结构

反编译识别出的唯一主类 `Widget`（继承 `QWidget`），`this` 指针下的成员布局：

| 偏移 | 类型 | 成员名 | 说明 |
|------|------|--------|------|
| +0x18 | `Ui::Widget*` | `ui` | Qt Designer 生成的 UI 对象 |
| +0x1c | `QSerialPort*` | `serialPort` | 串口对象 |
| +0x20 | `QTimer*` | `timer` | 串口列表刷新定时器（1000ms） |

> 说明：`ui` 结构体内部的子控件通过 `ui + 偏移` 访问，偏移量在 `.ui` 编译产物中固定，见下文「控件偏移表」。

## 2. 构造函数摘要（0x4043f0）

```
Widget::Widget(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

    serialPort = new QSerialPort(this);
    serialPort->setParity(QSerialPort::NoParity);        // 0
    serialPort->setDataBits(QSerialPort::Data8);          // 8
    serialPort->setStopBits(QSerialPort::OneStop);        // 1
    serialPort->setFlowControl(QSerialPort::NoFlowControl); // 0

    // 串口有数据到达 → 解析响应
    QObject::connect(serialPort, &QIODevice::readyRead,
                     this, &Widget::datareceived);

    timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout,
                     this, &Widget::refreshserial);
    timer->start(1000);  // 每秒刷新一次可用串口列表

    // 地址输入框限制 0..1500
    ui->lineEdit_addr->setValidator(new QIntValidator(0, 1500, this));

    // 载入并缩放 logo（220×220，保持宽高比）
    QPixmap logo(":/new/prefix1/logo.png");
    ui->label_logo->setPixmap(logo.scaled(220, 220, Qt::KeepAspectRatio));
}
```

## 3. 串口通信协议

### 3.1 帧格式（主机 → 设备）

```
0x7A | [addr] | [cmd] | [data(大端序, 可选)] | [checksum] | 0x7B
```

- 帧头 `0x7A`，帧尾 `0x7B`
- `checksum` = 从 `0x7A` 到最后一个数据字节的 **逐字节 XOR**（函数 `0x4017c0`）
- 多字节数据一律 **大端序**（高字节在前）

按数据长度分为四种帧：

| 帧长 | 结构 | 适用命令 |
|------|------|----------|
| 5 字节 | `7A addr cmd cs 7B` | 无数据命令 |
| 6 字节 | `7A addr cmd value cs 7B` | 8bit 数据（仅 cmd 0x0D 地址设置） |
| 7 字节 | `7A addr cmd d_hi d_lo cs 7B` | 16bit 数据，或 8bit+保留高字节 |
| 9 字节 | `7A addr cmd d[3] d[2] d[1] d[0] cs 7B` | 32bit 数据（仅 cmd 0x02 位置） |

### 3.2 命令码总表

> ⚠️ 命令「语义」以官方《F32C 使用手册》为准（本 exe 为 2026-04 旧版，界面标签与
> 现固件命令码有出入）：**0x01=转速、0x07=加速度、0x0D=地址设置**；0x04 已废弃。
> 下表「语义」列已按官方手册更正。

| cmd | 槽函数（地址） | 语义 | 数据宽度 | 帧长 |
|-----|----------------|------|----------|------|
| 0x00 | on_comboBox_4_activated (0x403750) | 电机控制模式选择 | mode(8bit) | 7 |
| 0x01 | on_lineEdit_10_returnPressed (0x402ee0) | 速度控制 | 16bit | 7 |
| 0x02 | on_lineEdit_6_returnPressed (0x403070) | 目标位置（多圈角度） | 32bit | 9 |
| 0x03 | on_lineEdit_11_returnPressed (0x403370) | 目标角度（单圈绝对位置） | 16bit | 7 |
| 0x04 | on_lineEdit_13_returnPressed (0x402940) | （废弃，官方手册未列出） | 16bit | 7 |
| 0x05 | on_pushButton_4_clicked (0x402670) | 地址设置 | 无(用addr) | 5 |
| 0x06 | on_pushButton_7_clicked (0x402df0) | （未识别） | 无 | 5 |
| 0x07 | on_lineEdit_14_returnPressed (0x402ad0) | 加速度控制 | 16bit | 7 |
| 0x08 | on_pushButton_3_clicked (0x403660) | 保存设备参数 | 无 | 5 |
| 0x0A | on_pushButton_5_clicked (0x402760) | （未识别） | 无 | 5 |
| 0x0B | on_pushButton_6_clicked (0x402850) | （未识别） | 无 | 5 |
| 0x0C | on_pushButton_2_clicked (0x402170) | 读取所有参数（addr=0xFF 广播） | 无 | 5 |
| 0x0D | on_lineEdit_15_returnPressed (0x402c60) | 地址设置 | 8bit | 6 |
| 0x0F | on_lineEdit_returnPressed (0x403b90) | 速度 PID KP | 8bit | 7 |
| 0x10 | on_lineEdit_2_returnPressed (0x403d10) | 速度 PID KI | 8bit | 7 |
| 0x11 | on_lineEdit_3_returnPressed (0x403e90) | 位置 PID KP | 8bit | 7 |
| 0x12 | on_lineEdit_16_returnPressed (0x404010) | 位置 PID KI | 8bit | 7 |

### 3.3 响应帧（设备 → 主机，20 字节）

`read all`（cmd 0x0C）触发设备回传 20 字节状态：

```
0x7A | cmd | 转过角度(4B,÷10) | 机械角度(2B,÷10) | 母线电压(2B,÷10)
     | 电机速度(2B,÷100) | 电流(2B, mA) | 速度PID KP(1B) | 速度PID KI(1B)
     | 位置PID KP(1B) | 位置PID KI(1B) | cs | 0x7B
```

| 偏移 | 字段 | 类型 | 处理 | 显示目标 |
|------|------|------|------|----------|
| 0 | 0x7A | 帧头 | — | — |
| 1 | 命令码 | 8bit | 原样 | lineEdit(0x1a0) |
| 2..5 | 转过角度 | 32bit BE | ÷10.0 | label(0x1ac) |
| 6..7 | 机械角度 | 16bit BE | ÷10.0 | label(0x1bc) |
| 8..9 | 母线电压 | 16bit BE | ÷10.0 | label(0x230) |
| 10..11 | 电机速度 | 16bit BE | ÷100.0 | label(0x1cc) |
| 12..13 | 电流 | 16bit BE | 原样(mA) | label(0x220) |
| 14 | 速度 PID KP | 8bit | 原样 | lineEdit(0x1e8) |
| 15 | 速度 PID KI | 8bit | 原样 | lineEdit(0x1f4) |
| 16 | 位置 PID KP | 8bit | 原样 | lineEdit(0x208) |
| 17 | 位置 PID KI | 8bit | 原样 | lineEdit(0x214) |
| 18 | checksum | — | XOR 校验 | — |
| 19 | 0x7B | 帧尾 | — | — |

## 4. 控件偏移表（ui 结构体内部）

这些偏移是反编译中从 `mov 0xNNN(%eax),%eax`（`ui` 指针）推得：

| ui 偏移 | 控件类型 | 作用 |
|---------|----------|------|
| 0x2c | QComboBox | 串口名选择（refreshserial 填充） |
| 0x34 | QComboBox | 数据位选择 |
| 0x38 | QComboBox | 波特率选择 |
| 0x3c | QComboBox | 停止位选择 |
| 0x40 | QComboBox | 校验位选择 |
| 0x48 | QPushButton | 连接 / 断开按钮 |
| 0x78 | QComboBox | 电机控制模式选择 |
| 0x98 | QLineEdit | lineEdit_10：加速度值 |
| 0xcc | QLineEdit | lineEdit_6：目标位置（多圈角度） |
| 0x100 | QLineEdit | lineEdit_11：目标角度（单圈） |
| 0x140 | QLineEdit | lineEdit_13：电机速度 |
| 0x164 | QLineEdit | lineEdit_14：cmd 0x07 参数 |
| 0x184 | QLineEdit | lineEdit_15：电流值 |
| 0x1a0 | QLineEdit | 地址设置（兼作所有命令的目标地址） |
| 0x1ac | QLabel | 转过角度（只读显示） |
| 0x1bc | QLabel | 机械角度（只读显示） |
| 0x1cc | QLabel | 电机速度（只读显示） |
| 0x1e8 | QLineEdit | 速度 PID KP |
| 0x1f4 | QLineEdit | 速度 PID KI |
| 0x208 | QLineEdit | 位置 PID KP |
| 0x214 | QLineEdit | 位置 PID KI |
| 0x220 | QLabel | 电流（只读显示） |
| 0x230 | QLabel | 母线电压（只读显示） |

## 5. 文件索引

| 文件 | 地址 | 说明 |
|------|------|------|
| `checksum.cpp` | 0x4017c0 | XOR 校验和计算 |
| `constructor.cpp` | 0x4043f0 | Widget 构造函数 |
| `refreshserial.cpp` | 0x404190 | 刷新串口列表（定时器槽） |
| `datareceived.cpp` | 0x401a00 | 串口数据解析（readyRead 槽） |
| `on_pushButton_clicked.cpp` | 0x4021f0 | 连接 / 断开 |
| `on_pushButton_2_clicked.cpp` | 0x402170 | 读取所有参数 (0x0C) |
| `on_pushButton_3_clicked.cpp` | 0x403660 | 保存设备参数 (0x08) |
| `on_pushButton_4_clicked.cpp` | 0x402670 | 地址设置 (0x05) |
| `on_pushButton_5_clicked.cpp` | 0x402760 | 命令 0x0A |
| `on_pushButton_6_clicked.cpp` | 0x402850 | 命令 0x0B |
| `on_pushButton_7_clicked.cpp` | 0x402df0 | 命令 0x06 |
| `on_comboBox_4_activated.cpp` | 0x403750 | 控制模式选择 |
| `on_lineEdit_10_returnPressed.cpp` | 0x402ee0 | 加速度 (0x01) |
| `on_lineEdit_6_returnPressed.cpp` | 0x403070 | 目标位置 (0x02) |
| `on_lineEdit_11_returnPressed.cpp` | 0x403370 | 目标角度 (0x03) |
| `on_lineEdit_13_returnPressed.cpp` | 0x402940 | 目标速度 (0x04) |
| `on_lineEdit_14_returnPressed.cpp` | 0x402ad0 | 命令 0x07 |
| `on_lineEdit_15_returnPressed.cpp` | 0x402c60 | 电流 (0x0D) |
| `on_lineEdit_returnPressed.cpp` | 0x403b90 | 速度 PID KP (0x0F) |
| `on_lineEdit_2_returnPressed.cpp` | 0x403d10 | 速度 PID KI (0x10) |
| `on_lineEdit_3_returnPressed.cpp` | 0x403e90 | 位置 PID KP (0x11) |
| `on_lineEdit_16_returnPressed.cpp` | 0x404010 | 位置 PID KI (0x12) |
| `qt_static_metacall.cpp` | 0x404940 | moc 生成的信号分派跳转表 |

## 6. 关键 IAT 映射（地址 → 函数）

| IAT 槽 | 符号 |
|--------|------|
| 0x423528 | QIODevice::write |
| 0x42352c | QIODevice::readAll |
| 0x423530 | QIODevice::readyRead |
| 0x423560 | QString::toInt |
| 0x423564 | QString::toDouble |
| 0x423558 | QString::arg(double) |
| 0x42355c | QString::arg(long long) |
| 0x42351c | QString::fromAscii_helper |
| 0x423518 | QString::compare_helper |
| 0x423628 | QLabel::setText |
| 0x42362c | QLabel::setPixmap |
| 0x423600 | QAbstractButton::setText |
| 0x423718 | QAbstractButton::text |
| 0x423710 | QLineEdit::setText |
| 0x423754 | QLineEdit::text |
| 0x42374c | QComboBox::currentText |
| 0x4236fc | QComboBox::setCurrentText |
| 0x4236f8 | QComboBox::insertItems |
| 0x423700 | QComboBox::setCurrentIndex |
| 0x423704 | QComboBox::clear |
| 0x423590/94/98/9c/a0/a8 | QSerialPort setBaudRate/setDataBits/setPortName/setStopBits/setFlowControl/setParity |
| 0x4235a4 | QSerialPort::clear |
| 0x4235b0/b8 | QSerialPortInfo::availablePorts/portName |
| 0x4234e8 | QTimer::start |
| 0x4234ec | QTimer::timeout |
| 0x4234fc | QObject::connectImpl |
| 0x42356c | QIntValidator ctor |
| 0x423580/88 | QPixmap ctor / scaled |
| 0x423570 | QDesktopServices::openUrl |
| 0x4235d8 | QMessageBox::warning |
