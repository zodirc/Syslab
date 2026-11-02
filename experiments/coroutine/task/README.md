# CO-002 coroutine-task

## 实验目的

两个协程在挂起点交替，日志为 ACBD。任务抛出 runtime_error 时，run 重新抛出该异常。

## 程序实际检查的内容

日志为 ACBD。第二个调度器的 run 抛出 runtime_error。

## 不由本实验断言的内容

两个协程在挂起点交替，日志为 ACBD。任务抛出 runtime_error 时，run 重新抛出该异常。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
