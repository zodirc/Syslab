# 预期行为

## 通过条件

新挂载命名空间内的 `vis/marker` 读回 `in`。进入该命名空间之前创建的进程看不到这个文件。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

用户命名空间或挂载命名空间不能建立时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不检查 `MS_PRIVATE` 以外的挂载传播，也不读取编排系统的描述文本。
