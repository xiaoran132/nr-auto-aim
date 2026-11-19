# Widget 构造函数分析（`Widget::Widget(QWidget*)` @ 0x4043F0）

## 1. 函数签名与调用约定

- 地址：`0x4043F0`
- 原型：`Widget::Widget(QWidget *parent)`（Qt 元对象系统 `this` 通过 `%ecx` 传递，GCC `thiscall`）
- 栈帧：`push %ebp / mov %esp,%ebp / sub $0x9c,%esp`（0x9C=156 字节局部空间）
- 保存寄存器：`%edi`、`%esi`、`%ebx`（被调用者保存）
- `%ebx` 在整个构造函数中固定保存 `this` 指针。

## 2. 逐步重建

| 序号 | 指令要点 | 含义 |
|------|---------|------|
| 1 | `call <0x4236F0>` | 调用基类 `QWidget::QWidget(QWidget*, flags)` 构造 |
| 2 | `movl $0x41C958,(%ebx)` | 写入主 vtable（`Widget` 的 vtable） |
| 3 | `movl $0x41CA30,0x8(%ebx)` | 写入次 vtable（`QObject` 第二基类子对象 vtable，多继承） |
| 4 | `movl $0x234,(%esp); call 0x405F60` | `operator new(0x234)` = 分配 564 字节的 `Ui::Widget` |
| 5 | `mov %eax,0x18(%ebx)` | 存入 `this->ui`（成员偏移 **+0x18**） |
| 6 | `call 0x408A30` | `setupUi(this)`（moc 生成的 UI 构建函数） |
| 7 | `movl $0x0,0x24(%ebx)` | `this->成员[+0x24] = 0`（一个标志/状态位，初始化为 0） |
| 8 | `call 0x405F60`（new 12） + `call <0x4235AC>` | `new QSerialPort(this)`，存 `this->serialPort`（**+0x1C**） |
| 9 | 串口配置（见 §3） | 设置校验位、数据位、停止位、流控 |
| 10 | `new QTimer`（24 字节）+ `call <0x4234F0>` | `new QTimer(this)`，存 `this->timer`（**+0x20**） |
| 11 | `call <0x4234FC>`（connectImpl）×2 | 建立两条信号连接（见 §4） |
| 12 | `mov $0x3E8,(%esp); call <0x4234E8>` | `timer->start(1000)` —— **定时器 1000ms（1 秒）周期** |
| 13 | `new QIntValidator(0,1500,this)` + `call <0x42370C>` | 给 `ui` 成员 `+0x1A0` 的 QLineEdit 挂数值校验器（见 §5） |

## 3. 串口配置（第 9 步）

`this->serialPort` 上的四个 setter，参数为**立即数**，与 Qt 枚举值对应：

| IAT 调用 | Qt 方法 | 立即数 | 枚举值 |
|----------|---------|--------|--------|
| `<0x4235A8>` | `QSerialPort::setParity(int)` | 0 | `NoParity`（无校验） |
| `<0x423594>` | `QSerialPort::setDataBits(int)` | 8 | `Data8`（8 数据位） |
| `<0x42359C>` | `QSerialPort::setStopBits(int)` | 1 | `OneStop`（1 停止位） |
| `<0x4235A0>` | `QSerialPort::setFlowControl(int)` | 0 | `NoFlowControl`（无流控） |

> **注意：构造函数中没有调用 `setBaudRate`。** 波特率在“连接”按钮处理函数
> （`0x4021F0`）里，从波特率下拉框 `currentText()` 经 `QString::toInt`（`0x423560`）
> 解析后调用 `setBaudRate`（`0x423590`）设置。

## 4. 信号连接（第 11 步）

两条 `QObject::connectImpl` 调用（`0x4234FC`），参数中携带静态元对象与信号/槽枚举：

| 发送者成员 | 信号（IAT） | 接收者 | 槽（地址） |
|-----------|-------------|--------|-----------|
| `serialPort`（+0x1C） | `QIODevice::readyRead`（`0x423530`） | `this` | `Widget::datareceived`（`0x401A00`） |
| `timer`（+0x20） | `QTimer::timeout`（`0x4234EC`） | `this` | `Widget::refreshserial`（`0x404190`） |

- 连接 1 使用 `QIODevice::staticMetaObject`（`0x423524`）
- 连接 2 使用 `QTimer::staticMetaObject`（`0x4234E4`）
- 连接后调用 `QMetaObject::Connection` 析构（`0x4234C4`）清理临时连接对象。

## 5. QIntValidator（第 13 步）

- `new QIntValidator(int min, int max, QObject *parent)`（`0x42356C`）
- 参数：`min = 0`，`max = 0x5DC = 1500`，`parent = this`
- 通过 `QLineEdit::setValidator`（`0x42370C`）应用到 `ui` 成员 `+0x1A0` 的 QLineEdit。

该 QLineEdit（`ui+0x1A0`）同时是 `datareceived` 中接收帧第 1 个 16 位字段的显示控件，
也是用户输入位置目标值的输入框（数值范围 0~1500）。

## 6. 成员布局（已确认）

| 偏移 | 成员 | 类型 | 初始化 |
|------|------|------|--------|
| +0x00 | vptr | `*vtable` | 0x41C958 |
| +0x08 | vptr（次基类） | `*vtable` | 0x41CA30 |
| +0x18 | `ui` | `Ui::Widget*` | `new Ui::Widget` + `setupUi` |
| +0x1C | `serialPort` | `QSerialPort*` | `new QSerialPort(this)` |
| +0x20 | `timer` | `QTimer*` | `new QTimer(this)`，`start(1000)` |
| +0x24 | `flag` | `int` | 0 |

## 7. QTimer 用途

`timer`（周期 1000ms）超时触发 `refreshserial`（`0x404190`）：
- `QSerialPortInfo::availablePorts()`（`0x4235B0`）枚举当前可用串口
- 用 `QComboBox::currentText()`（`0x42374C`）记住当前选中项
- `QSerialPortInfo::portName()`（`0x4235B8`）取每个端口名，刷新串口下拉框

即：**每秒轮询一次系统串口列表，动态刷新“串口”下拉框**（支持热插拔）。
