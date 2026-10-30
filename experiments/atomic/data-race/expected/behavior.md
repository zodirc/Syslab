# 预期行为

## 通过条件

四线程各递增 1000 次，计数为 4000。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

数据竞争是未定义行为。本实验不提供会触发竞争的默认路径，因此也不能用一次未崩溃证明没有竞争。
