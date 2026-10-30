# 预期行为

## 通过条件

catch (const Base&) 捕获 Derived，typeid 与 Derived 相等。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

异常表和 typeinfo 的二进制布局属于 ABI。本实验不比对具体符号名。
