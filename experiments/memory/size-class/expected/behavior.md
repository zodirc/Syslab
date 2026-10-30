# 预期行为

## 通过条件

24 字节和 8192 字节的分配都成功，释放后 allocated_bytes 为 0，且该字段被标记为已测量。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

大小类边界是实现选择。未置位的统计字段不是 0 这个测量值。
