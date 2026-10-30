# 预期行为

## 通过条件

移动构造之后源指针为空，新对象持有原地址且值为 4。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

拷贝构造已删除。本节不检查浅拷贝、深拷贝或标准库类型的移动。
