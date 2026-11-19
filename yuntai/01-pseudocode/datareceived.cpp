// ============================================================
// datareceived  —— 串口数据到达时的响应解析
// 地址：0x401a00
// 触发：QIODevice::readyRead（构造函数中连接）
// ============================================================
//
// 设备在收到「读取所有参数」(cmd 0x0C) 后回传 20 字节状态帧：
//
//   偏移    字段           类型     处理       显示目标(ui偏移)
//   ----    ----           ----     ----       ------------
//   0       0x7A           帧头     —          —
//   1       命令码         8bit     原样       lineEdit 0x1a0
//   2..5    转过角度       32bit BE ÷10.0      label 0x1ac
//   6..7    机械角度       16bit BE ÷10.0      label 0x1bc
//   8..9    母线电压       16bit BE ÷10.0      label 0x230
//   10..11  电机速度       16bit BE ÷100.0     label 0x1cc
//   12..13  电流           16bit BE 原样(mA)   label 0x220
//   14      速度PID KP     8bit     原样       lineEdit 0x1e8
//   15      速度PID KI     8bit     原样       lineEdit 0x1f4
//   16      位置PID KP     8bit     原样       lineEdit 0x208
//   17      位置PID KI     8bit     原样       lineEdit 0x214
//   18      checksum       —        XOR 校验   —
//   19      0x7B           帧尾     —          —

void Widget::datareceived()
{
    QByteArray data = serialPort->readAll();        // 0x42352c

    // ---- 帧校验 ----
    if (data.isEmpty())
        return;                                     // 0x401a87: 长度==0

    if ((uint8_t)data[0] != 0x7A)                   // 0x401a8e: 首字节 != 0x7A
        return;

    if ((uint8_t)data[data.size()-1] != 0x7B)       // 0x401ae6: 末字节 != 0x7B
        return;

    if (data.size() != 20)                          // 0x401b40: cmp $0x14
        return;

    // XOR 校验（0x401b12 调 0x4018d0 重算，与帧内 checksum 比较）
    uint8_t recv_cs = (uint8_t)data[18];            // 0x401af3: 0x7B 前一个字节
    uint8_t calc_cs = checksum((uint8_t*)data.constData(), 18); // 0x4018d0
    if (calc_cs != recv_cs)                          // 0x401b1d: cmp
        return;

    const uint8_t* f = (const uint8_t*)data.constData();

    // ---- 字段解析与显示 ----
    // 字段0：命令码回显（8bit，十进制）
    ui->lineEdit_addr->setText(                      // 0x1a0, 0x423710
        QString("%1").arg(f[1]));                    // 0x41106c, 0x42355c

    // 字段1：转过角度（32bit 大端序 ÷10.0）
    {
        int32_t v = (f[2] << 24) | (f[3] << 16) | (f[4] << 8) | f[5];
        ui->label_turnAngle->setText(                // 0x1ac, 0x423628
            QString("%1").arg((double)v / 10.0));    // fdivs 0x413128, 0x423558
    }

    // 字段2：机械角度（16bit 大端序 ÷10.0）
    {
        int16_t v = (f[6] << 8) | f[7];
        ui->label_mechAngle->setText(                // 0x1bc
            QString("%1").arg((double)v / 10.0));
    }

    // 字段3：母线电压（16bit 大端序 ÷10.0）
    {
        int16_t v = (f[8] << 8) | f[9];
        ui->label_busVoltage->setText(               // 0x230
            QString("%1").arg((double)v / 10.0));
    }

    // 字段4：电机速度（16bit 大端序 ÷100.0）
    {
        int16_t v = (f[10] << 8) | f[11];
        ui->label_speed->setText(                    // 0x1cc
            QString("%1").arg((double)v / 100.0));   // fdivs 0x41312c
    }

    // 字段5：电流（16bit 大端序，原样，单位 mA）
    {
        int16_t v = (f[12] << 8) | f[13];
        ui->label_current->setText(                  // 0x220
            QString("%1").arg(v));                   // 0x42355c(整型 arg)
    }

    // 字段6~9：PID 参数（各 8bit，回显到输入框）
    ui->lineEdit_speedKP->setText(QString("%1").arg(f[14]));  // 0x1e8
    ui->lineEdit_speedKI->setText(QString("%1").arg(f[15]));  // 0x1f4
    ui->lineEdit_posKP  ->setText(QString("%1").arg(f[16]));  // 0x208
    ui->lineEdit_posKI  ->setText(QString("%1").arg(f[17]));  // 0x214
}
