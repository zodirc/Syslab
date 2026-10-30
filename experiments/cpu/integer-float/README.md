# CPU-001 integer-float-representation

## 实验目的

确认当前编译器下带符号整数的补码表示，以及 float 在声明为 IEC 559 时的 1.0 编码。

## 程序实际检查的内容

int 类型的 -1 每个字节都是 0xff。若 numeric_limits<float>::is_iec559 为真，则 1.0f 的内存位型是 0x3f800000。

## 不由本实验断言的内容

本实验不把某一条浮点指令或寄存器分配写成必须出现的汇编。is_iec559 为假时结果是 unsupported，而不是改用推测编码。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
