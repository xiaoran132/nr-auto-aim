// ============================================================
// on_pushButton_2_clicked  —— 「读取参数」按钮
// 地址：0x402170
// 命令：0x0C（读取所有参数，addr=0xFF 广播）
// 帧：7A FF 0C cs 7B  （5 字节）
// ============================================================
//
// 反汇编确认：
//   frame[0]=0x7A, frame[1]=0xFF, frame[2]=0x0C, frame[3]=cs, frame[4]=0x7B
//   checksum(frame, 3)   // 0x40217b: movl $0x3
//   write(frame, 5)      // 0x4021b1: movl $0x5
//   waitForBytesWritten(1000)

void Widget::on_pushButton_2_clicked()
{
    uint8_t frame[5];
    frame[0] = 0x7A;
    frame[1] = 0xFF;              // 广播地址
    frame[2] = 0x0C;              // 读取所有参数
    frame[3] = checksum(frame, 3);
    frame[4] = 0x7B;

    serialPort->write((const char*)frame, 5);        // 0x423528
    serialPort->waitForBytesWritten(1000);           // vtable +0x60, 0x3e8
}
