# 整数、浮点和调用

C++20 规定有符号整数使用补码。CPU-001 检查 `int` 的 `-1` 是否每个字节都是 `0xff`。`float` 的 `1.0` 位型 `0x3f800000` 只在 `numeric_limits<float>::is_iec559` 为真时检查，否则实验返回 unsupported。

CPU-004 的汇编按 System V AMD64 ABI 或 AArch64 PCS 从前六个整数参数寄存器取数并相加，返回值为 21。

虚函数调用必须选择动态类型，这是 CPU-005 的检查。虚表布局和间接调用指令属于 ABI，只出现在反汇编观察中。
