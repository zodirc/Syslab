# CPU-004 sysv-aarch64-abi-sum

## 实验目的

abi_sum6 按 System V AMD64 ABI 或 AArch64 PCS 从前六个整数参数寄存器取数并相加，返回值为 21。

## 程序实际检查的内容

abi_sum6(1, 2, 3, 4, 5, 6) 返回 21。x86-64 汇编使用 rdi、rsi、rdx、rcx、r8、r9 和 rax。AArch64 汇编使用 x0 到 x5。

## 不由本实验断言的内容

其他编译器如何安排 C++ 函数的寄存器分配不由本文件规定。本实验只约束这个汇编入口。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
