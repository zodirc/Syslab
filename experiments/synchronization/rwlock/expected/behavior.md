# 预期行为

## 通过条件

写者递增 100 次，结果为 100。共享锁路径能够完成。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

写优先是这个实现的策略，不是 POSIX rwlock 的唯一合法策略。
