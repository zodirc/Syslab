# 内存序与回收

AT-001 的四个线程在互斥量保护下各递增 1000 次，计数为 4000。程序只运行加锁路径。

AT-002 的四个线程各执行 10000 次 `memory_order_relaxed` 的 `fetch_add`，计数为 40000。AT-003 在 200 次试验中，`memory_order_acquire` 看到旗标为 1 之后读到载荷 42。AT-006 用 `std::atomic_thread_fence` 的 release 与 acquire 配对，200 次试验中 acquire fence 之后的载荷为 42。

AT-004 的存储和加载使用 `memory_order_seq_cst`。300 次试验中两个加载没有同时为 0。AT-005 的加载和存储使用 `memory_order_seq_cst`。200 次试验中没有出现 r1=1、r2=0、r3=1、r4=0。

AT-006 核对的是 `std::atomic_thread_fence`。volatile 不替代原子操作。

AT-007 比较一个 uint64_t。版本从 0 改为 1 之后，期望值仍为 1 的比较交换失败，低 32 位保持为 1。AT-009 的弹出集合大小为 800。AT-010 的消费者依次得到 0 到 9999。AT-011 的互异值数量为 2000。AT-012 在槽仍公布地址时删除计数为 127，clear(0) 之后为 383。AT-013 在 pin 期间的删除计数为 0，unpin 之后为 2。
