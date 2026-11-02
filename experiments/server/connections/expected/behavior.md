# 预期行为

## 通过条件

两个处理线程的 `tid` 都大于 0 且互不相等，标记 `A` 和 `B` 都被读到。进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。

## 边界

本实验不描述 `task_struct`，也不把 `tid` 当作 PID 命名空间中的进程号。
