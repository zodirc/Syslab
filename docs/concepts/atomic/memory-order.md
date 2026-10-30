# 内存序与回收

AT-001 不执行非原子数据竞争。被互斥量保护的计数是有定义行为。一次没有崩溃不能证明竞争不存在，所以实验根本不跑竞争路径。

relaxed `fetch_add` 保证加法的原子性，不建立对其他普通写入的 happens-before。release 存储与 acquire 加载配对时，payload 必须可见。`atomic_thread_fence` 的 release/acquire 配对同样传递 relaxed 原子两侧的 payload。这三件事分别由 AT-002、AT-003 和 AT-006 检查。

seq_cst 存储缓冲实验不允许两个加载同时看到 0。relaxed 或 acq_rel 允许该结果，AT-004 不把那种允许的结果当成失败。IRIW 的禁止结果若在样本中出现则失败；没有出现只覆盖该次样本。

编译器屏障、C++ `atomic_thread_fence`、ISA 屏障和缓存一致性不是同义词。volatile 不替代原子操作。

Hazard pointer 在槽仍发布地址时不回收该地址。扫描在退役列表达到槽预算后发生。Epoch 在 pin 期间不删除刚退役的对象。Treiber stack、SPSC 和 MPMC 的进度声明依赖 `__atomic_always_lock_free`。ABA 实验是带标签字的确定性模拟，不是栈里的真实故障。
