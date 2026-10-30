# 预期行为

## 通过条件

sched_setaffinity 成功，随后的掩码包含 CPU 0。设置失败时结果为 unsupported。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

读回掩码不证明下一条指令就在 CPU 0 上退休。本实验不调用 sched_getcpu 作为通过条件。
