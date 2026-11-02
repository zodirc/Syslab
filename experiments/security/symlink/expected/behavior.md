# 预期行为

## 通过条件

`open` 读到 `DATA`。`readlink` 返回 `target.bin`。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

符号链接不能建立时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不检查多层链接。
