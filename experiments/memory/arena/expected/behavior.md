# 预期行为

## 通过条件

释放最后一次 32 字节分配后，下一次相同请求返回同一地址。随后的第二次释放抛出 syslab::Error。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

中间空洞不会被重新分配。该分配器不是线程安全的，也不替换全局 new。
