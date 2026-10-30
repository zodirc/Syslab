# 预期行为

## 通过条件

子进程因 SIGSEGV 终止。父进程继续并输出通过标记。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

信号的内核投递路径不由本实验结果命名。父进程不访问该 PROT_NONE 页。
