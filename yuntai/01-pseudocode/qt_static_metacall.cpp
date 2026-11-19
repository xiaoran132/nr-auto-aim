// ============================================================
// qt_static_metacall  —— Qt moc 生成的信号分派跳转表
// 地址：0x404940（跳转表数据在 0x41c080，thunk 在 0x404950~0x404a90）
// ============================================================
//
// 反汇编关键事实：
//   0x404940: cmp $0x13,%edx        ; _id 越界检查（>19 返回）
//   0x404945: jmp *0x41c080(,%edx,4) ; switch(_id) 的跳转表
//
// 跳转表项（索引 _id → 槽函数）：
//   _id  地址       槽函数（命令码）
//   ---  ---------  --------------------------------
//   0    0x403e90   on_lineEdit_3_returnPressed  (0x11)
//   1    0x403d10   on_lineEdit_2_returnPressed  (0x10)
//   2    0x403b90   on_lineEdit_returnPressed    (0x0F)
//   3    0x403750   on_comboBox_4_activated      (模式选择, 带 int 参数)
//   4    0x403660   on_pushButton_3_clicked      (0x08)
//   5    0x403370   on_lineEdit_11_returnPressed (0x03)
//   6    0x403070   on_lineEdit_6_returnPressed  (0x02)
//   7    0x402ee0   on_lineEdit_10_returnPressed (0x01)
//   8    0x402df0   on_pushButton_7_clicked      (0x06)
//   9    0x402c60   on_lineEdit_15_returnPressed (0x0D)
//   10   0x402ad0   on_lineEdit_14_returnPressed (0x07)
//   11   0x402940   on_lineEdit_13_returnPressed (0x04)
//   12   0x402850   on_pushButton_6_clicked      (0x0B)
//   13   0x402760   on_pushButton_5_clicked      (0x0A)
//   14   0x402670   on_pushButton_4_clicked      (0x05)
//   15   0x402170   on_pushButton_2_clicked      (0x0C)
//   16   0x4021f0   on_pushButton_clicked        (连接/断开)
//   17   0x401a00   datareceived
//   18   0x404190   refreshserial
//
// 注：_id == 3（on_comboBox_4_activated）为唯一带参数的槽，
//     thunk 从 _a[1] 取 int 参数传入（QComboBox::activated(int) 信号）。

void Widget::qt_static_metacall(QObject* _o,
                                QMetaObject::Call _c,
                                int _id,
                                void** _a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Widget* _t = static_cast<Widget*>(_o);
        switch (_id) {
        case 0:  _t->on_lineEdit_3_returnPressed();  break;
        case 1:  _t->on_lineEdit_2_returnPressed();  break;
        case 2:  _t->on_lineEdit_returnPressed();    break;
        case 3:  _t->on_comboBox_4_activated((*reinterpret_cast<int*>(_a[1]))); break;
        case 4:  _t->on_pushButton_3_clicked();      break;
        case 5:  _t->on_lineEdit_11_returnPressed(); break;
        case 6:  _t->on_lineEdit_6_returnPressed();  break;
        case 7:  _t->on_lineEdit_10_returnPressed(); break;
        case 8:  _t->on_pushButton_7_clicked();      break;
        case 9:  _t->on_lineEdit_15_returnPressed(); break;
        case 10: _t->on_lineEdit_14_returnPressed(); break;
        case 11: _t->on_lineEdit_13_returnPressed(); break;
        case 12: _t->on_pushButton_6_clicked();      break;
        case 13: _t->on_pushButton_5_clicked();      break;
        case 14: _t->on_pushButton_4_clicked();      break;
        case 15: _t->on_pushButton_2_clicked();      break;
        case 16: _t->on_pushButton_clicked();        break;
        case 17: _t->datareceived();                 break;
        case 18: _t->refreshserial();                break;
        default: break;
        }
    }
}
