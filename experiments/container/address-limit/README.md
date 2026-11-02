# CTR-004 address-limit

## 实验目的

确认 `RLIMIT_AS` 的软限制等于当前 `VmSize` 加上 64MiB 时，余量内的匿名映射可以写入，超过余量的映射返回 `MAP_FAILED`。

## 程序实际检查的内容

从 `/proc/self/status` 读取 `VmSize`。软限制设为该值加上 64MiB。一页 `MAP_PRIVATE|MAP_ANONYMOUS` 映射写入一个字节。长度为 80MiB 的映射返回 `MAP_FAILED`。硬限制低于这个软限制，或 `setrlimit` 失败时，结果为不支持。

## 不由本实验断言的内容

运行器把 `SIGXCPU` 记为 `resource_limit`。本实验以 `MAP_FAILED` 为检查结果，不依赖那个终止原因。`MEM-007` 核对映射建立之后的读写。

反汇编、系统调用跟踪、ELF 元数据和耗时进入本次运行的观察记录。工具不存在或本次没有采样时，记录的状态是 unavailable 或 unsupported，程序不补造这些数据。
