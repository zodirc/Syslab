# 预期行为

## 通过条件

两个节点离开作用域后析构次数为 2，weak 过期。移动后的 UniquePtr 持有值 4，原指针为空。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

若两边都保存 SharedPtr，对象会留下来。本实验故意不构造那个循环，并在说明里指出这一点。
