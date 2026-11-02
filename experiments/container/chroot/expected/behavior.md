# 预期行为

## 通过条件

`chroot` 之后 `/data/token` 的内容是 `inside`，`/outside` 打开失败。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

用户命名空间、挂载命名空间或 `chroot` 不能完成时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不切换其他文件系统，也不读取编排系统的描述文本。
