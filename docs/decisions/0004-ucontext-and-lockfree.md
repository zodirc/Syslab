# 0004. Fiber 使用 ucontext，无锁查询不链接 libatomic

## 状态

已接受。

## 背景

手写汇编上下文切换需要按 ABI 保存被调用者保存寄存器并验证栈对齐。设计允许先使用已有的上下文切换机制。本机的 `libatomic` 链接脚本指向不存在的 `libatomic.so.1.2.0`，`std::atomic::is_lock_free()` 会引用其中的符号。

## 决定

Fiber 使用 `ucontext`。栈用匿名映射分配，从而满足 `mprotect` 的页对齐要求；低地址一页设为不可访问。

`LockFreeStack`、`SpscRing` 和 `MpmcQueue` 的 `is_lock_free()` 调用 `__atomic_always_lock_free`。结果为假时，进度声明不成立，实验返回 unsupported。

## 后果

这不是生产调度器，也不能在信号处理函数中调用。16 字节原子没有经过验证，ABA 实验使用 8 字节标记字，避免依赖 `cmpxchg16b` 的运行时库。
