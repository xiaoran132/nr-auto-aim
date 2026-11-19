; =============================================================================
; BLDC-Control.exe — 关键函数反汇编注解（AT&T 语法，objdump 输出）
; IAT 地址已解析为 demangle 后的 Qt 符号（见行内注释）。
; 完整 17174 行反汇编见 ..\_work\dis.txt / dis_annotated.txt
; =============================================================================

; -----------------------------------------------------------------------------
; Widget::Widget(QWidget*)  @ 0x4043F0
; this 经 %ecx 传入（GCC thiscall）；%ebx 全程保存 this
; -----------------------------------------------------------------------------
004043F0:  55                      push   %ebp
004043F1:  89 e5                   mov    %esp,%ebp
004043F3:  57                      push   %edi
004043F4:  56                      push   %esi
004043F5:  53                      push   %ebx
004043F6:  89 cb                   mov    %ecx,%ebx          ; ebx = this
004043F8:  81 ec 9c 00 00 00       sub    $0x9c,%esp         ; 156 字节局部
004043FE:  8b 45 08                mov    0x8(%ebp),%eax     ; eax = parent
00404401:  c7 44 24 04 00 00 00 00 movl   $0x0,0x4(%esp)     ; flags = 0
0040440C:  ff 15 f0 36 42 00       call   <0x4236F0>         ; QWidget::QWidget(parent, flags)

; --- 写入 vtable（Qt 多继承：主 vtable + QObject 次基类 vtable）---
00404415:  c7 03 58 c9 41 00       movl   $0x41C958,(%ebx)    ; this->vptr = 0x41C958
0040441B:  c7 43 08 30 ca 41 00    movl   $0x41CA30,0x8(%ebx) ; this->vptr2 = 0x41CA30

; --- 分配 Ui::Widget（0x234 = 564 字节）并 setupUi ---
00404422:  c7 04 24 34 02 00 00    movl   $0x234,(%esp)       ; 564
00404429:  e8 32 1b 00 00          call   0x405F60            ; operator new(564)
0040442E:  89 43 18                mov    %eax,0x18(%ebx)     ; this->ui = new Ui::Widget  (+0x18)
00404436:  e8 f5 45 00 00          call   0x408A30            ; setupUi(this)

; --- 标志位清零 ---
0040443E:  c7 43 24 00 00 00 00    movl   $0x0,0x24(%ebx)     ; this->flag = 0  (+0x24)

; --- new QSerialPort(this) ---
0040444C:  e8 0f 1b 00 00          call   0x405F60            ; operator new(12)
00404458:  ff 15 ac 35 42 00       call   <0x4235AC>          ; QSerialPort::QSerialPort(parent)
00404461:  89 73 1c                mov    %esi,0x1c(%ebx)     ; this->serialPort = ... (+0x1C)

; --- 串口参数配置（无 setBaudRate，波特率在“连接”时设置）---
0040446D:  ff 15 a8 35 42 00       call   <0x4235A8>          ; setParity(0)    = NoParity
00404476:  8b 4b 1c                mov    0x1c(%ebx),%ecx     ; this->serialPort
00404480:  ff 15 94 35 42 00       call   <0x423594>          ; setDataBits(8)  = Data8
00404489:  8b 4b 1c                mov    0x1c(%ebx),%ecx
00404493:  ff 15 9c 35 42 00       call   <0x42359C>          ; setStopBits(1)  = OneStop
0040449C:  8b 4b 1c                mov    0x1c(%ebx),%ecx
004044A6:  ff 15 a0 35 42 00       call   <0x4235A0>          ; setFlowControl(0) = NoFlowControl

; --- 连接 1: serialPort.readyRead -> datareceived ---
004044AC:  a1 30 35 42 00          mov    0x423530,%eax       ; signal: QIODevice::readyRead
004044B4:  8b 73 1c                mov    0x1c(%ebx),%esi     ; sender = serialPort
004044FE:  8b 15 24 35 42 00       mov    0x423524,%edx       ; QIODevice::staticMetaObject
00404527:  a1 fc 34 42 00          mov    0x4234FC,%eax       ; QObject::connectImpl
00404536:  ff d0                   call   *%eax               ; connect(serialPort, readyRead, this, datareceived)

; --- new QTimer(this) ---
00404545:  c7 04 24 18 00 00 00    movl   $0x18,(%esp)        ; 24 字节
0040454C:  e8 0f 1a 00 00          call   0x405F60            ; operator new(24)
0040455B:  ff 15 f0 34 42 00       call   <0x4234F0>          ; QTimer::QTimer(parent)
00404569:  89 73 20                mov    %esi,0x20(%ebx)     ; this->timer = ... (+0x20)

; --- 连接 2: timer.timeout -> refreshserial ---
00404561:  a1 ec 34 42 00          mov    0x4234EC,%eax       ; signal: QTimer::timeout
004045B6:  8b 15 e4 34 42 00       mov    0x4234E4,%edx       ; QTimer::staticMetaObject
004045EC:  ff d0                   call   *%eax               ; connect(timer, timeout, this, refreshserial)

; --- 启动定时器 1000ms ---
004045F5:  8b 4b 20                mov    0x20(%ebx),%ecx     ; this->timer
004045F8:  c7 04 24 e8 03 00 00    movl   $0x3E8,(%esp)       ; 1000
004045FF:  ff 15 e8 34 42 00       call   <0x4234E8>          ; QTimer::start(1000)

; --- new QIntValidator(0, 1500, this)，挂到 ui+0x1A0 的 QLineEdit ---
00404608:  c7 04 24 10 00 00 00    movl   $0x10,(%esp)        ; 16 字节
0040460F:  e8 4c 19 00 00          call   0x405F60            ; operator new(16)
00404614:  89 5c 24 08             mov    %ebx,0x8(%esp)      ; parent = this
00404618:  c7 44 24 04 dc 05 00 00 movl   $0x5DC,0x4(%esp)    ; max = 1500
00404622:  c7 04 24 00 00 00 00    movl   $0x0,(%esp)         ; min = 0
0040462C:  ff 15 6c 35 42 00       call   <0x42356C>          ; QIntValidator::QIntValidator(0,1500,this)
00404632:  8b 43 18                mov    0x18(%ebx),%eax     ; this->ui
00404638:  8b 88 a0 01 00 00       mov    0x1A0(%eax),%ecx    ; ui->成员(+0x1A0) = QLineEdit
00404644:  ff 15 0c 37 42 00       call   <0x42370C>          ; QLineEdit::setValidator(...)

; -----------------------------------------------------------------------------
; Widget::datareceived()  @ 0x401A00 —— 单字段解析块（F1，byte[2..3]，16 位大端）
; 10 个字段的代码结构完全相同，仅字节偏移/目标控件偏移不同，此处取 F1 代表。
; -----------------------------------------------------------------------------
; 帧长校验
00401B40:  83 ff 14                cmp    $0x14,%edi          ; 20 字节

; --- 读 byte[2..3]，拼 16 位大端 ---
00401BE9:  0f b6 4c 3a 02          movzbl 0x2(%edx,%edi,1),%ecx  ; byte[2] 高字节
00401BEE:  0f b6 44 3a 03          movzbl 0x3(%edx,%edi,1),%eax  ; byte[3] 低字节
00401BF3:  c1 e1 08                shl    $0x8,%ecx              ; 高字节 << 8
; ... 合并成 16 位值存入临时变量 ...

; --- QString::arg(long long, 0, base=10) 格式化为十进制字符串 ---
00401B93:  c1 fa 1f                sar    $0x1f,%edx             ; 符号扩展 64 位
00401B8B:  c7 44 24 10 0a 00 00 00 movl   $0xa,0x10(%esp)        ; base = 10
00401BA6:  8b 15 5c 35 42 00       mov    0x42355C,%edx          ; QString::arg(qlonglong,int,int,QChar)
00401BB4:  ff d2                   call   *%edx

; --- QLineEdit::setText(ui->成员, 字符串) ---
00401BB9:  8b 15 10 37 42 00       mov    0x423710,%edx          ; QLineEdit::setText
00401BC2:  8b 40 18                mov    0x18(%eax),%eax        ; this->ui
00401BC7:  8b 88 a0 01 00 00       mov    0x1A0(%eax),%ecx       ; ui->成员(+0x1A0)
00401BD0:  ff d2                   call   *%edx                  ; setText("…")

; --- 清理临时 QString（析构 ×2）---
00401BD7:  e8 f4 47 00 00          call   0x4063D0               ; QString::~QString
00401BDE:  e8 ed 47 00 00          call   0x4063D0               ; QString::~QString

; 其余字段（F2~F10）为相同模板，映射见 datareceived_analysis.md：
;   F2 byte[4..5]  -> ui+0x1AC   (arg(double,'g'))
;   F3 byte[6..7]  -> ui+0x1BC   (arg(double,'g'))
;   F4 byte[8..9]  -> ui+0x230   (arg(double,'g'))
;   F5 byte[10..11]-> ui+0x1CC   (arg(double,'g'))
;   F6 byte[12..13]-> ui+0x220   (arg(double,'g'))
;   F7 byte[14]    -> ui+0x1E8   (8 位)
;   F8 byte[15]    -> ui+0x1F4   (8 位)
;   F9 byte[16]    -> ui+0x208   (8 位)
;   F10 byte[17]   -> ui+0x214   (8 位)

; -----------------------------------------------------------------------------
; 校验和计算（0x4017C0 内，XOR 累加）
; -----------------------------------------------------------------------------
00401814:  0f b6 43 ff             movzbl -0x1(%ebx),%eax   ; 当前字节
00401818:  30 45 c7                xor    %al,-0x39(%ebp)    ; 累加器 ^= 字节
0040181B:  39 5d bc                cmp    %ebx,-0x44(%ebp)   ; 是否到帧尾
; 从 0x7A 到最后一个数据字节逐字节异或，得到 1 字节校验和。

; -----------------------------------------------------------------------------
; Widget::refreshserial()  @ 0x404190 —— QTimer 每秒轮询串口列表
; -----------------------------------------------------------------------------
004041A1:  a1 38 35 42 00          mov    0x423538,%eax       ; QListData::shared_null
004041A9:  a1 c0 34 42 00          mov    0x4234C0,%eax       ; QArrayData::shared_null
004041BD:  ff 15 4c 37 42 00       call   <0x42374C>          ; QComboBox::currentText()  记住选中项
004041F7:  ff 15 b0 35 42 00       call   <0x4235B0>          ; QSerialPortInfo::availablePorts()
00404257:  ff 15 b8 35 42 00       call   <0x4235B8>          ; QSerialPortInfo::portName()  逐端口取名字
; 用枚举到的端口名刷新“串口”下拉框，支持热插拔。

; =============================================================================
; 附：本函数涉及的 IAT 符号（demangle 后）
; =============================================================================
; 0x4236F0 QWidget::QWidget(QWidget*, QFlags<Qt::WindowType>)
; 0x4235AC QSerialPort::QSerialPort(QObject*)
; 0x4234F0 QTimer::QTimer(QObject*)
; 0x4234E8 QTimer::start(int)
; 0x4234FC QObject::connectImpl(...)
; 0x423530 QIODevice::readyRead      (信号)
; 0x4234EC QTimer::timeout          (信号)
; 0x423524 QIODevice::staticMetaObject
; 0x4234E4 QTimer::staticMetaObject
; 0x4234C4 QMetaObject::Connection::~Connection()
; 0x4235A8 setParity   0x423594 setDataBits   0x42359C setStopBits   0x4235A0 setFlowControl
; 0x42356C QIntValidator::QIntValidator(int,int,QObject*)
; 0x42370C QLineEdit::setValidator(QValidator const*)
; 0x423710 QLineEdit::setText(QString const&)
; 0x423558 QString::arg(double,int,char,int,QChar)
; 0x42355C QString::arg(qlonglong,int,int,QChar)
; 0x4235B0 QSerialPortInfo::availablePorts()
; 0x4235B8 QSerialPortInfo::portName()
; 0x42374C QComboBox::currentText()
