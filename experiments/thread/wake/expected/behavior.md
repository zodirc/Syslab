# 预期行为

## 通过条件

等待线程从 wait 返回后把 observed 写成 true。join 之后 observed 为 true。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本节的谓词读取 ready。通过条件是 join 之后 observed 为 true。
