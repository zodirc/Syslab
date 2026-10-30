# 预期行为

## 通过条件

constant_result() 返回 6。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

编译器可以保留乘法，也可以折叠成立即数。两种代码都满足语言规则。本实验不因缺少某条指令而失败。
