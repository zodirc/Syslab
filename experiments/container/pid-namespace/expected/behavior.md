# 预期行为

## 通过条件

用户命名空间把当前用户映射为 0，`unshare(CLONE_NEWPID)` 之后第一个子进程的 pid 为 1。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

用户命名空间或 PID 命名空间不能建立时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本实验不检查挂载命名空间、chroot 或编排系统的描述文本。
