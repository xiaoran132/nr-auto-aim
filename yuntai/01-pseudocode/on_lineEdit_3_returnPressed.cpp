// ============================================================
// on_lineEdit_3_returnPressed  —— 位置 PID KP 输入框回车
// 地址：0x403e90
// 命令：0x11（设置位置环 KP，8bit）
// 帧：7A [addr] 11 00 [KP] cs 7B  （7 字节）
// ============================================================
//
// 反汇编确认：
//   读 lineEdit 0x1a0（地址）→ toInt → frame[1]
//   读 lineEdit 0x208（位置 PID KP）→ toInt → 低字节 frame[4]
//   frame[3] 恒为 0x00
//   checksum(frame, 5)，write(frame, 7)，waitForBytesWritten(1000)

void Widget::on_lineEdit_3_returnPressed()
{
    uint8_t frame[7];
    frame[0] = 0x7A;
    frame[1] = (uint8_t)ui->lineEdit_addr->text().toInt();    // 0x1a0
    frame[2] = 0x11;              // 设置位置 PID KP
    frame[3] = 0x00;
    frame[4] = (uint8_t)ui->lineEdit_posKP->text().toInt();   // 0x208
    frame[5] = checksum(frame, 5);
    frame[6] = 0x7B;

    serialPort->write((const char*)frame, 7);
    serialPort->waitForBytesWritten(1000);
}
