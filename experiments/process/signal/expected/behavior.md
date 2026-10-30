# 预期行为

## 通过条件

sig_atomic_t 计数变为 1。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

处理函数只写 sig_atomic_t。系统调用跟踪是观察，不要求固定的 rt_sigaction 参数。
