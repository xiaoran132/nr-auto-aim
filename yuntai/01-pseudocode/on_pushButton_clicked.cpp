// ============================================================
// on_pushButton_clicked  —— 连接 / 断开切换
// 地址：0x4021f0
// ============================================================
//
// 逻辑：按钮当前文本为「断开」→ 执行断开；
//       否则（「连接」）→ 配置串口并打开。
//
// 反汇编确认：
//   QAbstractButton::text(0x48) → QString::compare("断开")
//   断开分支：QSerialPort::clear(3) → close() → setText("连接") → setEnabled(true)
//   连接分支：clear(3) → setPortName(0x2c) → setBaudRate(0x38 toInt)
//             → open(ReadWrite=0x3)
//             成功 → setText("断开") + setEnabled(false)
//             失败 → QMessageBox::warning("提示", "串口打开失败!")

void Widget::on_pushButton_clicked()
{
    QString text = ui->pushButton_conn->text();      // 0x48, 0x423718

    // ---- 断开 ----
    if (text == QString::fromAscii("断开")) {        // 0x41106f, 0x423518
        serialPort->clear(QSerialPort::AllDirections); // 0x4235a4, 参数 3
        serialPort->close();                          // vtable +0x38
        ui->pushButton_conn->setText("连接");         // 0x411076, 0x423600
        ui->comboBox_serial->setEnabled(true);        // 0x2c, 0x423648
        return;
    }

    // ---- 连接 ----
    serialPort->clear(QSerialPort::AllDirections);     // 0x4235a4, 参数 3

    serialPort->setPortName(                           // 0x423598
        ui->comboBox_serial->currentText());           // 0x2c, 0x42374c

    serialPort->setBaudRate(                           // 0x423590
        ui->comboBox_baud->currentText().toInt());     // 0x38, 0x423560

    // open(QIODevice::ReadWrite = 0x3)  —— 0x402413 虚调用
    if (serialPort->open(QIODevice::ReadWrite)) {
        ui->pushButton_conn->setText("断开");         // 0x41106f, 0x423600
        ui->comboBox_serial->setEnabled(false);        // 0x2c, 0x423648
    } else {
        QMessageBox::warning(                          // 0x4235d8
            this,
            QString::fromAscii("提示"),                // 0x411091
            QString::fromAscii("串口打开失败!"));      // 0x41107d
    }
}
