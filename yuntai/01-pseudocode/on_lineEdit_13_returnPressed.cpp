// ============================================================
// on_lineEdit_13_returnPressed  —— 命令 0x04（官方手册未列出，勿用）
// 地址：0x402940
// 命令：0x04（16bit 大端序）。官方手册无 0x04，疑似旧协议；转速应为 0x01
// 帧：7A [addr] 04 [d_hi] [d_lo] cs 7B  （7 字节）
// ============================================================
//
// 反汇编确认：
//   读 lineEdit 0x1a0（地址）→ toInt → frame[1]
//   读 lineEdit 0x140（电机速度）→ toDouble → fistpl(int32)
//   frame[3] = value>>8（0x402a19: sar $0x8），frame[4] = value&0xFF
//   checksum(frame, 5)，write(frame, 7)，waitForBytesWritten(1000)

void Widget::on_lineEdit_13_returnPressed()
{
    uint8_t frame[7];
    frame[0] = 0x7A;
    frame[1] = (uint8_t)ui->lineEdit_addr->text().toInt();   // 0x1a0
    frame[2] = 0x04;              // 官方手册未列出，勿用（转速=0x01）

    int16_t value = (int16_t)ui->lineEdit_speed->text().toDouble(); // 0x140
    frame[3] = (uint8_t)(value >> 8) & 0xFF;
    frame[4] = (uint8_t)(value & 0xFF);

    frame[5] = checksum(frame, 5);
    frame[6] = 0x7B;

    serialPort->write((const char*)frame, 7);
    serialPort->waitForBytesWritten(1000);
}
