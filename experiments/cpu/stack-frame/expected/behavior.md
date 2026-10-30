# 预期行为

## 通过条件

volatile int 的值仍是 7，它的地址非空。

进程退出码为 0，标准输出包含一行 `SYSLAB_RESULT status=pass`。主机无法完成该实验时，程序输出 `SYSLAB_RESULT status=unsupported`，运行器把结果记为 UNSUPPORTED。

## 边界

帧指针是否保留、栈槽是否被合并，取决于编译器和优化级别，不是 C++ 标准规定的固定布局。比较两个变体的反汇编时要同时看 build.json。
