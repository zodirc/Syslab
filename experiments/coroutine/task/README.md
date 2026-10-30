# CO-002 coroutine-task

## 实验目的

检查两个协程任务按挂起点交替，并检查协程异常从 run 重新抛出。

## 程序实际检查的内容

日志为 ACBD。第二个调度器的 run 抛出 runtime_error。

## 不由本实验断言的内容

协程不自动并行。取消只在 yield 恢复点观察，本实验不调用 cancel。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
