# 教学分配器

六种分配器实现 `IAllocator`。`allocate` 在耗尽时返回空指针。`deallocate(nullptr)` 是空操作。重复释放、外来指针和损坏的 canary 抛出 `syslab::Error`。大小为 0 或对齐不是 2 的幂时抛出 `InvalidArgument`。

除 `ThreadCacheAllocator` 外，这些分配器不是线程安全的。线程缓存按精确请求大小分箱，跨线程释放回到上游并持有互斥量。

Arena 只按后进先出回退尾部。Pool 的 `free_bytes` 是空闲块的用户容量。Buddy 的映射长度保持所请求的 2 的幂，空闲字节由空闲链表重算。Size-class 在类耗尽或请求超过最大类时使用单独映射。

统计字段以 `available_fields` 为准。未置位的计数不是测量值。
