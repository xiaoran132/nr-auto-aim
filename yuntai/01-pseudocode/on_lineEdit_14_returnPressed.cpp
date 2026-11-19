// ============================================================
// on_lineEdit_14_returnPressed  —— 命令 0x07（官方手册=加速度控制）
// 地址：0x402ad0
// 命令：0x07（16bit 大端序，加速度，单位 转/s²）
// 帧：7A [addr] 07 [d_hi] [d_lo] cs 7B  （7 字节）
// ============================================================
//
// 反汇编确认：
//   读 lineEdit 0x1a0（地址）→ toInt → frame[1]
//   读 lineEdit 0x164（未知参数）→ toDouble → fistpl(int32)
//   frame[3] = value>>8，frame[4] = value&0xFF
//   checksum(frame, 5)，write(frame, 7)，waitForBytesWritten(1000)
//
// 语义：官方手册 0x07=加速度控制（此控件即「加速度值」输入框）。

void Widget::on_lineEdit_14_returnPressed()
{
    uint8_t frame[7];
    frame[0] = 0x7A;
    frame[1] = (uint8_t)ui->lineEdit_addr->text().toInt();   // 0x1a0
    frame[2] = 0x07;

    int16_t value = (int16_t)ui->lineEdit_14->text().toDouble(); // 0x164
    frame[3] = (uint8_t)(value >> 8) & 0xFF;
    frame[4] = (uint8_t)(value & 0xFF);

    frame[5] = checksum(frame, 5);
    frame[6] = 0x7B;

    serialPort->write((const char*)frame, 7);
    serialPort->waitForBytesWritten(1000);
}
