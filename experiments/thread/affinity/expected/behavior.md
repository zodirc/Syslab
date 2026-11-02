# 预期行为

## 通过条件

sched_setaffinity 返回 0，随后 sched_getaffinity 读回的掩码包含 CPU 0。sched_setaffinity 返回非 0 时，结果为不支持。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本节核对读回的掩码。sched_setaffinity 失败时，结果为不支持。
