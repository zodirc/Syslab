# 编译器可以改变的部分

CPU-008 的函数在抽象机中得到 6。编译器可以保留乘法，也可以做常量折叠。两种代码都通过。是否折叠要对比 Debug 与 Release 的反汇编和 `build.json`，不能从返回值推断。

CPU-003 让一个 `volatile` 局部变量保持可寻址。帧指针和栈槽是否保留取决于优化级别，README 要求比较两个变体的反汇编，而不是要求某一种帧布局。

RT-001 对标准布局类型使用 `offsetof`。非标准布局和虚表不使用该宏作为语言结论。`write_layout` 会打印 `standard_layout` 和 `polymorphic`。
