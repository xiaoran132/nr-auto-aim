// ============================================================
// on_pushButton_4_clicked  —— 「电机失能」按钮
// 地址：0x402670
// 命令：0x05（电机失能 / disable）
// 帧：7A [addr] 05 cs 7B  （5 字节）
// ============================================================
//
// 反汇编确认：
//   frame[0]=0x7A, frame[1]=地址值, frame[2]=0x05, frame[3]=cs, frame[4]=0x7B
//   读 lineEdit 0x1a0（地址设置）→ toInt → 低字节写入 frame[1]
//   0x4026be: mov %al,-0xc(%ebp)  = frame[1] = 地址
//   0x4026f3: mov %al,-0xa(%ebp)  = frame[3] = checksum

void Widget::on_pushButton_4_clicked()
{
    uint8_t frame[5];
    frame[0] = 0x7A;
    frame[1] = (uint8_t)ui->lineEdit_addr->text().toInt();  // 0x1a0
    frame[2] = 0x05;              // 电机失能
    frame[3] = checksum(frame, 3);
    frame[4] = 0x7B;

    serialPort->write((const char*)frame, 5);
    serialPort->waitForBytesWritten(1000);
}
