# 预期行为

## 通过条件

pin 期间退役一个对象后删除计数为 0。unpin 后再退役两个对象，删除计数为 2。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

三个对象里有一个在最后一次 retire 返回时仍留在延迟列表中。本节只 pin 一个 EpochDomain。
