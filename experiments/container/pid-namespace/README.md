# CTR-001 pid-namespace

## 实验目的

确认当前用户映射为用户命名空间内的 0 之后，`unshare(CLONE_NEWPID)` 创建的第一个子进程在新 PID 命名空间里的 pid 是 1。

## 程序实际检查的内容

用户命名空间的 `uid_map` 和 `gid_map` 写入成功，随后 `unshare(CLONE_NEWPID)` 成功时，第一个子进程的 `getpid()` 等于 1。用户命名空间或 PID 命名空间不能建立时，程序输出 `SYSLAB_RESULT status=unsupported`。

## 不由本实验断言的内容

本实验不检查挂载命名空间和 chroot，也不读取编排系统的描述文本。命名空间不能建立时，结果保持为不支持。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
