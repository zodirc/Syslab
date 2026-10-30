# 预期行为

## 通过条件

两个 SYS_gettid 的结果都为正，且不相等。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

线程创建在内核中的函数名随版本变化，不写入通过条件。strace 只作为观察。
