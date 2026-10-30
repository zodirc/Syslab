# 预期行为

## 通过条件

每个线程分配并释放 40 个 32 字节块。结束后 allocated_bytes 为 0。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

缓存按精确请求大小分箱。跨线程释放回到上游。这不是生产分配器，也不替换全局 new。
