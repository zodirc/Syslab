# 预期行为

## 通过条件

三次约 1000 字节的分配全部释放后，2500 字节的请求成功。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

合并策略和最小分割余量是这个教学实现的选择。统计字段以 available_fields 为准。
