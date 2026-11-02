# 预期行为

## 通过条件

同一节点在 unlock 返回后再次加锁成功。四个线程各递增 1000 次，计数为 4000。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本节不构造 unlock 返回前销毁节点的用例。一次未崩溃不能代替那个用例。
