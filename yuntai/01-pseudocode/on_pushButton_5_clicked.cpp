// ============================================================
// on_pushButton_5_clicked  —— 命令 0x0A（功能未识别）
// 地址：0x402760
// 命令：0x0A
// 帧：7A [addr] 0A cs 7B  （5 字节）
// ============================================================
//
// 反汇编确认：
//   frame[0]=0x7A, frame[1]=地址, frame[2]=0x0A, frame[3]=cs, frame[4]=0x7B
//   读 lineEdit 0x1a0（地址设置）→ toInt → 低字节写入 frame[1]
//   0x4027ae: mov %al,-0xc(%ebp)  = frame[1]
//   0x4027e3: mov %al,-0xa(%ebp)  = frame[3] = checksum
//
// 语义推测：根据 UI 文本顺序，可能对应「电机使能」。
// （无数据字段，仅带目标地址，属开关类命令。）

void Widget::on_pushButton_5_clicked()
{
    uint8_t frame[5];
    frame[0] = 0x7A;
    frame[1] = (uint8_t)ui->lineEdit_addr->text().toInt();  // 0x1a0
    frame[2] = 0x0A;
    frame[3] = checksum(frame, 3);
    frame[4] = 0x7B;

    serialPort->write((const char*)frame, 5);
    serialPort->waitForBytesWritten(1000);
}
