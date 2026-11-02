# 预期行为

## 通过条件

恢复后的页是 `KEEPTAIL`，`applied_lsn` 为 2。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

`fsync` 失败时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不生成模糊检查点，也不处理多页脏页表。
