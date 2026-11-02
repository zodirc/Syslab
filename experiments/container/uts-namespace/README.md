# CTR-005 uts-namespace

## 实验目的

确认新 UTS 命名空间中的主机名可以设为 `syslab-ns`，命名空间外的进程仍读到原来的主机名。

## 程序实际检查的内容

`fork` 发生在 `unshare(CLONE_NEWUTS)` 之前。用户命名空间映射为 0 之后，本进程 `sethostname("syslab-ns")` 并读回该名字。命名空间外进程的 `gethostname` 与进入之前的值相同。用户命名空间或 UTS 命名空间不能建立时，结果为不支持。

## 不由本实验断言的内容

本实验不修改域名解析结果，也不检查 PID 命名空间。用户命名空间的映射见 `CTR-001`。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
