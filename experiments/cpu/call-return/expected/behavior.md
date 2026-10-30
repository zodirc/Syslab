# 预期行为

## 通过条件

add_six(1, 2, 3, 4, 5, 6) 的返回值等于 21。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不检查参数位于 rdi 还是 x0。那是 ABI 观察，由 CPU-004 的独立汇编文件做数值闭合，反汇编只保留现场。
