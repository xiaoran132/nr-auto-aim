// ============================================================
// on_comboBox_4_activated  —— 电机控制模式选择
// 地址：0x403750
// 命令：0x00（设置控制模式）
// 帧：7A [addr] 00 00 [mode] cs 7B  （7 字节）
// ============================================================
//
// 反汇编确认：
//   读 lineEdit 0x1a0（地址）→ toInt → frame[1]
//   读 comboBox 0x78（模式）→ currentText → 与 5 个字符串逐个比较
//   匹配后把 mode 值(0..4) 写入 frame[4]，frame[3] 恒为 0
//   checksum(frame, 5)，write(frame, 7)，waitForBytesWritten(1000)
//
// 模式映射（按 QString::compare 顺序）：
//   "速度控制"              -> mode 0
//   "多圈位置控制-T"         -> mode 1
//   "单圈绝对位置控制-T"      -> mode 2
//   "多圈位置控制-直通"       -> mode 3
//   "单圈绝对位置控制-直通"    -> mode 4

void Widget::on_comboBox_4_activated()
{
    uint8_t frame[7];
    frame[0] = 0x7A;
    frame[1] = (uint8_t)ui->lineEdit_addr->text().toInt();  // 0x1a0
    frame[2] = 0x00;              // 设置控制模式

    QString sel = ui->comboBox_mode->currentText();  // 0x78, 0x42374c
    int mode;
    if      (sel == "速度控制")             mode = 0;  // 0x411104
    else if (sel == "多圈位置控制-T")       mode = 1;  // 0x411098
    else if (sel == "单圈绝对位置控制-T")   mode = 2;  // 0x4110c7
    else if (sel == "多圈位置控制-直通")    mode = 3;  // 0x4110ad
    else                                   mode = 4;  // 0x4110e4 "单圈绝对位置控制-直通"

    frame[3] = 0x00;
    frame[4] = (uint8_t)mode;
    frame[5] = checksum(frame, 5);
    frame[6] = 0x7B;

    serialPort->write((const char*)frame, 7);
    serialPort->waitForBytesWritten(1000);
}
