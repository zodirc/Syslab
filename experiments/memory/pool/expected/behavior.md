# 预期行为

## 通过条件

四次 32 字节请求返回非空。第 5 次 32 字节请求和 64 字节请求返回空指针。deallocate 一块之后，32 字节请求返回非空。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本节构造 PoolAllocator(32, 4, 16)。第 5 次 32 字节请求和 64 字节请求返回空指针。
