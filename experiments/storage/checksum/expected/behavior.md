# 预期行为

## 通过条件

校验和相符时返回 `RECORD01`。校验和不符时不返回载荷，调用方缓冲区保持不变。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

`fsync` 失败时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不恢复被撕开的页，也不解释块设备的扇区原子性。
