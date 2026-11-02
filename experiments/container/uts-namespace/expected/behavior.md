# 预期行为

## 通过条件

新 UTS 命名空间的主机名是 `syslab-ns`。命名空间外的主机名保持进入之前的值。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

用户命名空间或 UTS 命名空间不能建立时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不修改域名解析，也不检查 PID 命名空间。
