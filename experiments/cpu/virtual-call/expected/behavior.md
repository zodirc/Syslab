# 预期行为

## 通过条件

Base* 指向 Derived 对象时，value() 返回 7。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

Base* 指向 Derived 对象时，value() 返回 7。
