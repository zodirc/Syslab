# 预期行为

## 通过条件

pin 期间退役一个对象后删除计数为 0。unpin 后再退役两个对象，删除计数为 2。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

最后一个对象可以留到下一次宽限期。本域每个线程索引按域记录，一个线程同时 pin 两个域会覆盖索引，实验不那样使用。
