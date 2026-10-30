# 预期行为

## 通过条件

通过 Base* 调用 Derived::value 得到 7。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

虚表的内存布局、间接调用使用的具体寄存器属于 Itanium C++ ABI 和编译器实现，本实验只把反汇编作为观察。
