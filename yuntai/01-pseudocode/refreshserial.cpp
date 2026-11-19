// ============================================================
// refreshserial  —— 刷新可用串口列表
// 地址：0x404190
// 触发：QTimer::timeout（每 1000ms，构造函数中连接）
// ============================================================
//
// 反汇编确认的调用序列：
//   QComboBox::currentText(0x2c)      -> 保存当前选中串口名
//   QSerialPortInfo::availablePorts() -> 枚举系统串口
//   QSerialPortInfo::portName()       -> 循环取每个串口名
//   QComboBox::clear(0x2c)            -> 清空下拉框
//   QComboBox::insertItems(0x2c)      -> 插入串口名列表
//   QStringList::contains()           -> 判断旧选中项是否仍在
//   QComboBox::setCurrentText(0x2c)   -> 若仍在则恢复选中

void Widget::refreshserial()
{
    // 记住当前选中项，刷新后尽量恢复
    QString current = ui->comboBox_serial->currentText();  // 0x2c, 0x42374c

    // 枚举可用串口
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts(); // 0x4235b0
    QStringList names;
    for (const QSerialPortInfo& info : ports) {
        names << info.portName();                            // 0x4235b8
    }

    // 重建下拉框内容
    ui->comboBox_serial->clear();                            // 0x423704
    ui->comboBox_serial->insertItems(0, names);              // 0x4236f8

    // 恢复旧选中项（若该串口仍存在）
    if (names.contains(current)) {                           // 0x423544
        ui->comboBox_serial->setCurrentText(current);        // 0x4236fc
    }
}
