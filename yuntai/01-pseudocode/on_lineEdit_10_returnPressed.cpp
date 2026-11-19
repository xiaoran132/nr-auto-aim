// ============================================================
// on_lineEdit_10_returnPressed  —— 命令 0x01（官方手册=转速控制）
// 地址：0x402ee0
// 命令：0x01（16bit 大端序）。exe 界面标「加速度值」已过时，官方手册 0x01=转速
// 帧：7A [addr] 01 [d_hi] [d_lo] cs 7B  （7 字节）
// ============================================================
//
// 反汇编确认：
//   读 lineEdit 0x1a0（地址）→ toInt → frame[1]
//   读 lineEdit 0x98（加速度值）→ toDouble → fistpl(int32)
//   frame[3] = (value>>8)，frame[4] = value&0xFF
//   checksum(frame, 5)，write(frame, 7)，waitForBytesWritten(1000)

void Widget::on_lineEdit_10_returnPressed()
{
    uint8_t frame[7];
    frame[0] = 0x7A;
    frame[1] = (uint8_t)ui->lineEdit_addr->text().toInt();   // 0x1a0
    frame[2] = 0x01;              // 转速控制（官方手册 0x01=速度）

    int16_t value = (int16_t)ui->lineEdit_accel->text().toDouble(); // 0x98
    frame[3] = (uint8_t)(value >> 8) & 0xFF;   // 高字节
    frame[4] = (uint8_t)(value & 0xFF);        // 低字节

    frame[5] = checksum(frame, 5);
    frame[6] = 0x7B;

    serialPort->write((const char*)frame, 7);
    serialPort->waitForBytesWritten(1000);
}
