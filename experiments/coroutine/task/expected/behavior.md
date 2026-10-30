# 预期行为

## 通过条件

日志为 ACBD。第二个调度器的 run 抛出 runtime_error。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

协程不自动并行。取消只在 yield 恢复点观察，本实验不调用 cancel。
