# 预期行为

## 通过条件

throw Derived() 之后，catch (const Base&) 接到对象。typeid(object) 与 typeid(Derived) 相等。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

本节的通过条件是 typeid(object) 与 typeid(Derived) 相等。
