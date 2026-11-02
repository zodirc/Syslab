# 预期行为

## 通过条件

完整日志重放后页为 `ABCDEFGH`，`applied_lsn` 为 2。日志截断到第一条已提交记录后再重放，页为 `ABCD....`，`applied_lsn` 为 1。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

`fsync` 失败时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不检查组提交、检查点或多页恢复。
