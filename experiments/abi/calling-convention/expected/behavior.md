# 预期行为

## 通过条件

abi_sum6(1, 2, 3, 4, 5, 6) 返回 21。x86-64 汇编使用 rdi、rsi、rdx、rcx、r8、r9 和 rax。AArch64 汇编使用 x0 到 x5。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

其他编译器如何安排 C++ 函数的寄存器分配不由本文件规定。本实验只约束这个汇编入口。
