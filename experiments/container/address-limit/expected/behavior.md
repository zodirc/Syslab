# 预期行为

## 通过条件

64MiB 余量内的一页映射可以写入。80MiB 的映射返回 `MAP_FAILED`。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

硬限制不允许设置该软限制时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验的结果是 `MAP_FAILED`。运行器的 `resource_limit` 终止原因来自 `SIGXCPU`。
