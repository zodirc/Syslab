# 对象、协程和 fiber

`UniquePtr` 独占对象。`SharedPtr` 使用原子引用计数。`WeakPtr` 不延长对象寿命。MEM-009 不构造双向强引用循环；若构造了，对象不会在作用域结束时销毁。

协程任务的 `initial_suspend` 和 `final_suspend` 都是 `suspend_always`。`spawn` 取得帧的所有权并放入就绪队列。`yield_to` 把协程重新入队。`cancel` 只在下一次恢复该等待点时抛出。从协程逃出的异常由 `run` 重新抛出。协程不是线程。

Fiber 使用 POSIX `ucontext`。栈来自页对齐的匿名映射，低地址一页为 `PROT_NONE`。`yield` 只返回调度器，不创建内核线程，也不能从信号处理函数调用。不让出的 fiber 不会被抢占。

工作窃取的 `parallel_for` 给每个工人自己的双端队列。任务体抛出的异常在 `join` 之后重新抛出。单次切片大于队列容量时抛出 `ResourceLimit`。
