# CPU-009 atomic-instruction-observation

## 实验目的

在单线程里检查原子读改写的数值，并把不同 memory_order 的指令形式留在反汇编中。

## 程序实际检查的内容

relaxed 存入 1，seq_cst 存入 2，fetch_add(3) 的返回值是 2，随后的值是 5。

## 不由本实验断言的内容

本实验不要求出现 lock 前缀、xchg 或 dmb。指令选择依赖编译器和 ISA。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
