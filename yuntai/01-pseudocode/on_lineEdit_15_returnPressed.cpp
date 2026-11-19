// ============================================================
// on_lineEdit_15_returnPressed  —— 命令 0x0D（官方手册=地址设置）
// 地址：0x402c60
// 命令：0x0D（8bit，地址设置）。exe 界面标「电流值」已过时，官方手册 0x0D=改地址
// 帧：7A [addr] 0D [value] cs 7B  （6 字节，注意无数据高字节）
// ============================================================
//
// 反汇编确认：
//   读 lineEdit 0x1a0（地址）→ toInt → frame[1]
//   读 lineEdit 0x184（电流值）→ toDouble → fistps(int16) → 低字节 frame[3]
//   checksum(frame, 4)   // 0x402d3b: movl $0x4
//   write(frame, 6)      // 0x402d57: movl $0x6
//
// 注意：此命令是唯一 6 字节帧（8bit 数据直接跟 checksum，
//       与 0x0F~0x12 的 7 字节帧不同——后者保留一个 0x00 高字节）。

void Widget::on_lineEdit_15_returnPressed()
{
    uint8_t frame[6];
    frame[0] = 0x7A;
    frame[1] = (uint8_t)ui->lineEdit_addr->text().toInt();   // 0x1a0
    frame[2] = 0x0D;              // 地址设置（官方手册 0x0D=改地址）
    frame[3] = (uint8_t)ui->lineEdit_current->text().toDouble(); // 0x184, 8bit
    frame[4] = checksum(frame, 4);
    frame[5] = 0x7B;

    serialPort->write((const char*)frame, 6);
    serialPort->waitForBytesWritten(1000);
}
