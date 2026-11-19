// ============================================================
// on_pushButton_7_clicked  —— 「电机使能」按钮
// 地址：0x402df0
// 命令：0x06（电机使能 / enable，LED 闪烁实测证实）
// 帧：7A [addr] 06 cs 7B  （5 字节）
// ============================================================
//
// 反汇编确认：
//   frame[0]=0x7A, frame[1]=地址, frame[2]=0x06, frame[3]=cs, frame[4]=0x7B
//   读 lineEdit 0x1a0（地址设置）→ toInt → 低字节写入 frame[1]
//   0x402e3e: mov %al,-0xc(%ebp)  = frame[1]
//   0x402e73: mov %al,-0xa(%ebp)  = frame[3] = checksum
//
// 语义：电机使能（enable）。0x05 是失能，两者成对。

void Widget::on_pushButton_7_clicked()
{
    uint8_t frame[5];
    frame[0] = 0x7A;
    frame[1] = (uint8_t)ui->lineEdit_addr->text().toInt();  // 0x1a0
    frame[2] = 0x06;
    frame[3] = checksum(frame, 3);
    frame[4] = 0x7B;

    serialPort->write((const char*)frame, 5);
    serialPort->waitForBytesWritten(1000);
}
