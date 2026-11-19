// ============================================================
// on_lineEdit_11_returnPressed  —— 「目标位置（单圈角度）」输入框回车
// 地址：0x403370
// 命令：0x03（设置角度，16bit 大端序）
// 帧：7A [addr] 03 [d_hi] [d_lo] cs 7B  （7 字节）
// ============================================================
//
// 反汇编确认：
//   读 comboBox 0x78（模式）→ 判断分支（与 0x403070 同构）
//   读 lineEdit 0x1a0（地址）→ toInt → frame[1]
//   读 lineEdit 0x100（目标位置）→ toDouble → fistpl(int32)
//   frame[3] = value>>8，frame[4] = value&0xFF
//   checksum(frame, 5)，write(frame, 7)，waitForBytesWritten(1000)

void Widget::on_lineEdit_11_returnPressed()
{
    uint8_t frame[7];
    frame[0] = 0x7A;
    frame[1] = (uint8_t)ui->lineEdit_addr->text().toInt();   // 0x1a0
    frame[2] = 0x03;              // 设置角度

    int16_t value = (int16_t)ui->lineEdit_angle->text().toDouble(); // 0x100
    frame[3] = (uint8_t)(value >> 8) & 0xFF;
    frame[4] = (uint8_t)(value & 0xFF);

    frame[5] = checksum(frame, 5);
    frame[6] = 0x7B;

    serialPort->write((const char*)frame, 7);
    serialPort->waitForBytesWritten(1000);
}
