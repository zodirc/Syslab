# 预期行为

## 通过条件

allocate({0}) 抛出 syslab::Error。deallocate(nullptr) 不抛出。对同一块的第二次 deallocate 抛出 syslab::Error。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本节使用 ArenaAllocator。0 字节的 allocate 抛出 syslab::Error。deallocate(nullptr) 不抛出。同一块的第二次 deallocate 抛出 syslab::Error。
