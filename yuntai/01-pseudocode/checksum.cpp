// ============================================================
// checksum  —— XOR 校验和计算
// 地址：0x4017c0  （自由函数，cdecl 约定）
// ============================================================
//
// 功能：对帧数据逐字节异或，返回累加结果的低 8 位。
// 协议要求：checksum = 从帧头 0x7A 起，到最后一个数据字节止，
//           所有字节的 XOR（不含帧尾 0x7B，也不含 checksum 本身）。
//
// 反汇编关键事实：
//   movb $0x0, -0x39(%ebp)   ; checksum 累加器初始化为 0
//   movzbl -0x1(%ebx),%eax   ; 取当前字节
//   xor   %al, -0x39(%ebp)   ; checksum ^= byte
//   循环直到 (data + len) 结束
//
// 注：该函数体内还残留 QTextStream/QDebug 打印（QTextStream::operator<<
//     对 char / int，以及 QDebug 析构），属开发期调试遗留代码，
//     不影响 XOR 结果，此处省略。

uint8_t checksum(uint8_t* data, int len)
{
    uint8_t cs = 0;                     // -0x39(%ebp)
    if (len <= 0)                       // 0x4017ce: jle 返回
        return cs;

    for (int i = 0; i < len; i++) {     // 0x40180b..0x40181e 循环
        cs ^= data[i];                  // 0x401814..0x401818
    }
    return cs;                          // AL
}

// ------------------------------------------------------------
// 调用约定说明（供帧构造代码参考）：
//   checksum(帧指针, 覆盖字节数)
//   - 5 字节帧：checksum(frame, 3)   // 覆盖 0x7A, addr, cmd
//   - 7 字节帧：checksum(frame, 5)   // 覆盖 0x7A, addr, cmd, d_hi, d_lo
//   - 9 字节帧：checksum(frame, 7)   // 覆盖 0x7A, addr, cmd, 4 字节数据
// ------------------------------------------------------------
