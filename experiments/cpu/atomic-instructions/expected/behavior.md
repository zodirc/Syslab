# 预期行为

## 通过条件

relaxed 存入 1，seq_cst 存入 2，fetch_add(3) 的返回值是 2，随后的值是 5。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不要求出现 lock 前缀、xchg 或 dmb。指令选择依赖编译器和 ISA。
