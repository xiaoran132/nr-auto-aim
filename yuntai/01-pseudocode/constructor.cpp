// ============================================================
// Widget 构造函数
// 地址：0x4043f0
// ============================================================
//
// 成员布局（this 相对偏移）：
//   +0x18  Ui::Widget*  ui
//   +0x1c  QSerialPort* serialPort
//   +0x20  QTimer*      timer
//
// 反汇编确认的调用序列：
//   QSerialPort ctor  -> setParity(0) -> setDataBits(8)
//   -> setStopBits(1) -> setFlowControl(0)
//   QTimer ctor -> connect(timeout) -> start(1000)
//   QIntValidator(0, 1500) -> setValidator(lineEdit 0x1a0)
//   QPixmap(":/new/prefix1/logo.png") -> scaled(220,220) -> setPixmap(label 0x114)
//   installEventFilter(label 0x114)

Widget::Widget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::Widget)
{
    ui->setupUi(this);

    // ---- 串口对象 ----
    serialPort = new QSerialPort(this);               // 0x4235ac, 存 +0x1c
    serialPort->setParity(QSerialPort::NoParity);       // 0x4235a8, 参数 0
    serialPort->setDataBits(QSerialPort::Data8);        // 0x423594, 参数 8
    serialPort->setStopBits(QSerialPort::OneStop);      // 0x42359c, 参数 1
    serialPort->setFlowControl(QSerialPort::NoFlowControl); // 0x4235a0, 参数 0

    // ---- 信号连接 ----
    QObject::connect(serialPort, &QIODevice::readyRead,
                     this, &Widget::datareceived);      // datareceived = 0x401a00

    timer = new QTimer(this);                           // 0x4234f0, 存 +0x20
    QObject::connect(timer, &QTimer::timeout,
                     this, &Widget::refreshserial);     // refreshserial = 0x404190
    timer->start(1000);                                 // 0x4234e8, 0x3e8 = 1000

    // ---- 地址输入框验证器（0..1500）----
    ui->lineEdit_addr->setValidator(                    // lineEdit 0x1a0
        new QIntValidator(0, 1500, this));              // 0x42356c, 0 / 0x5dc

    // ---- logo（点击打开淘宝店）----
    QPixmap logo(":/new/prefix1/logo.png");             // 0x4130ef, 0x423580
    ui->label_logo->setPixmap(                          // label 0x114, 0x42362c
        logo.scaled(220, 220, Qt::KeepAspectRatio));    // 0x423588, 0xdc=220
    ui->label_logo->installEventFilter(this);           // 0x423514
}

// ============================================================
// Widget::eventFilter  —— logo 点击处理（构造函数 installEventFilter 的回调）
// （Qt 虚函数，构造函数中被 installEventFilter 引用）
// ============================================================
bool Widget::eventFilter(QObject* obj, QEvent* ev)
{
    // 点击 logo label -> 打开淘宝店铺
    if (obj == ui->label_logo && ev->type() == QEvent::MouseButtonPress) {
        QDesktopServices::openUrl(                       // 0x423570
            QUrl("https://minibalance.taobao.com/"));    // 0x413108, 0x4234d8
    }
    return QWidget::eventFilter(obj, ev);
}
