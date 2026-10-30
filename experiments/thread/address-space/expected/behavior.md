# 预期行为

## 通过条件

线程把 shared 写成 1，join 之后父线程读到 1。子进程修改 isolated 后，父进程仍读到 1。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

fork 之后虚拟地址数值常常相同，这不表示物理页相同。本实验不断言地址数值或 COW。
