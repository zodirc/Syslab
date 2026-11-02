# 进程与线程

`fork` 之后子进程的写入不会出现在父进程的同一对象中。OS-002 检查这一点，并把子进程 `ru_minflt` 记成度量。缺页计数不单独证明 Copy-on-Write。

同一进程的线程能看见彼此的普通写入。OS-004 把这件事和 fork 的隔离放在同一次运行里。tid 不同只说明 Linux 任务标识不同，不描述 `task_struct`。

OS-005 的等待者持锁调用 `wait`，谓词读取 `ready`。通知方持锁把 `ready` 写成 `true` 并调用 `notify_one`。等待者返回后，`observed` 为 `true`。

OS-006 调用 `sched_setaffinity`，把调用线程的掩码设为只含 CPU 0。`sched_getaffinity` 读回的掩码包含 CPU 0。设置失败时，结果为不支持。
