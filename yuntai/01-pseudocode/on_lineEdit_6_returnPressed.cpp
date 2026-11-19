// ============================================================
// on_lineEdit_6_returnPressed  —— 「目标角度（多圈位置）」输入框回车
// 地址：0x403070
// 命令：0x02（设置位置，32bit 大端序）
// 帧：7A [addr] 02 [d3] [d2] [d1] [d0] cs 7B  （9 字节）
// ============================================================
//
// 反汇编确认：
//   读 comboBox 0x78（模式）→ 用于判断控制模式分支
//   读 lineEdit 0x1a0（地址）→ toInt → frame[1]
//   读 lineEdit 0xcc（目标位置）→ toDouble → fistpl(int32)
//   frame[3] = value>>24，frame[4] = value>>16，
//   frame[5] = value>>8， frame[6] = value&0xFF
//   checksum(frame, 7)   // 0x403205: movl $0x7
//   write(frame, 9)      // 0x40323f: movl $0x9

void Widget::on_lineEdit_6_returnPressed()
{
    uint8_t frame[9];
    frame[0] = 0x7A;
    frame[1] = (uint8_t)ui->lineEdit_addr->text().toInt();   // 0x1a0
    frame[2] = 0x02;              // 设置位置（32bit）

    int32_t value = (int32_t)ui->lineEdit_pos->text().toDouble(); // 0xcc
    frame[3] = (uint8_t)((value >> 24) & 0xFF);  // 最高字节
    frame[4] = (uint8_t)((value >> 16) & 0xFF);
    frame[5] = (uint8_t)((value >> 8) & 0xFF);
    frame[6] = (uint8_t)( value & 0xFF);         // 最低字节

    frame[7] = checksum(frame, 7);
    frame[8] = 0x7B;

    serialPort->write((const char*)frame, 9);
    serialPort->waitForBytesWritten(1000);
}
