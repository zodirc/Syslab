# 预期行为

## 通过条件

两个协程在挂起点交替，日志为 ACBD。任务抛出 runtime_error 时，run 重新抛出该异常。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

两个协程在挂起点交替，日志为 ACBD。任务抛出 runtime_error 时，run 重新抛出该异常。
