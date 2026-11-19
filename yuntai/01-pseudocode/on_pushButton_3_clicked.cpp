// ============================================================
// on_pushButton_3_clicked  —— 「保存设备参数」按钮
// 地址：0x403660
// 命令：0x08（保存参数到设备）
// 帧：7A [addr] 08 cs 7B  （5 字节）
// ============================================================
//
// 反汇编确认：
//   frame[0]=0x7A, frame[1]=地址, frame[2]=0x08, frame[3]=cs, frame[4]=0x7B
//   读 lineEdit 0x1a0（地址设置）→ toInt → 低字节写入 frame[1]
//   checksum(frame, 3)，write(frame, 5)，waitForBytesWritten(1000)

void Widget::on_pushButton_3_clicked()
{
    uint8_t frame[5];
    frame[0] = 0x7A;
    frame[1] = (uint8_t)ui->lineEdit_addr->text().toInt();  // 0x1a0, 0x423754/0x423560
    frame[2] = 0x08;              // 保存设备参数
    frame[3] = checksum(frame, 3);
    frame[4] = 0x7B;

    serialPort->write((const char*)frame, 5);
    serialPort->waitForBytesWritten(1000);
}
