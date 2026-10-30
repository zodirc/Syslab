# 预期行为

## 通过条件

子进程把自己的 value 改为 2 后正常退出。父进程的 value 仍为 1。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

ru_minflt 只是度量。它为 0 或不为 0 都不单独证明物理页是否复制。
