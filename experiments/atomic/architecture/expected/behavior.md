# 预期行为

## 通过条件

x86-64 或 AArch64 宏恰好有一个成立。其他架构返回 unsupported。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

一次运行不能对照另一套指令集。允许行为的差异要在对应架构的构建上分别记录。
