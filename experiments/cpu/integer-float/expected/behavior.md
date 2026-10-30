# 预期行为

## 通过条件

int 类型的 -1 每个字节都是 0xff。若 numeric_limits<float>::is_iec559 为真，则 1.0f 的内存位型是 0x3f800000。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不把某一条浮点指令或寄存器分配写成必须出现的汇编。is_iec559 为假时结果是 unsupported，而不是改用推测编码。
