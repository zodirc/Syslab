# CTR-001 pid-namespace

## 实验目的

用户命名空间映射为 0 之后，CLONE_NEWPID 的第一个子进程 pid 是 1。命名空间不能建立时，结果为不支持。

## 程序实际检查的内容

用户命名空间的 `uid_map` 和 `gid_map` 写入成功，随后 `unshare(CLONE_NEWPID)` 成功时，第一个子进程的 `getpid()` 等于 1。用户命名空间或 PID 命名空间不能建立时，程序输出 `SYSLAB_RESULT status=unsupported`。

## 不由本实验断言的内容

用户命名空间映射为 0 之后，CLONE_NEWPID 的第一个子进程 pid 是 1。命名空间不能建立时，结果为不支持。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
