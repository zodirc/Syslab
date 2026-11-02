# 平台支持

manifest 的 `platform.os` 必须是 `linux`。`architectures` 只接受 `x86_64` 和 `aarch64`。当前开发主机是 x86-64，AArch64 源码路径有条件编译，但没有在 AArch64 机器或 QEMU 上运行。

CPU-004 的 `abi_sum.S` 按架构选择寄存器：x86-64 使用 System V 的前六个整数参数寄存器，AArch64 使用 PCS 的 x0 到 x5。数值检查是返回值 21，不是某条机器码。

协程需要 Clang 在没有 `<coroutine>` 时找到 polyfill 里的 `std::experimental::coroutine_traits`。有标准 `<coroutine>` 时 CMake 不再加入该包含路径。

`libatomic.so` 在本开发主机上不可用。无锁结构的 `is_lock_free()` 使用 `__atomic_always_lock_free`，不调用会引入 `libatomic` 的 `std::atomic::is_lock_free`。指针宽度或 `size_t` 不是始终无锁时，相关实验返回 unsupported。

环境快照中的编译器版本字段固定为 unavailable，原因是版本由工具链探测和 `build.json` 分别记录，避免两处互相覆盖成空字符串。同一次快照还记录 `cpufreq_governor`、`/proc/loadavg` 的前三个字段，以及运行进程继承的 CPU 亲和性。本机 governor 文件不存在，状态为 unavailable，原因是 `scaling_governor is not exposed`。负载和亲和性在 `CPU-002` 的 `environment.json` 中为 ok。

`perf` 不在本机 `PATH` 中。`probe_perf_events` 因此把 `perf_events` 标为 unavailable，不试跑计数器，也不写 0。`OS-007` 的观测 `complete` 为 false，限制说明是 `which did not resolve the command`。

本机直接调用 `unshare(CLONE_NEWPID)` 返回 `EPERM`。`CTR-001` 先 `unshare(CLONE_NEWUSER)`，把当前用户写入 `uid_map` 和 `gid_map` 为命名空间内的 0，再建立 PID 命名空间。2026-11-02 的运行记录在 `artifacts/runs/batch-20261102T030808-46d436b7`，结果 PASSED，第一个子进程的 pid 是 1。同一映射之后，`CTR-002` 在私有挂载命名空间里挂载 tmpfs，记录 `artifacts/runs/batch-20261102T030815-cb1baf90`；`CTR-003` 在该命名空间中 `chroot`，记录 `artifacts/runs/batch-20261102T030823-e44f3fa4`。两次结果都是 PASSED。`CTR-004` 把 `RLIMIT_AS` 软限制设为当时的 `VmSize` 加 64MiB，一页映射可以写入，80MiB 映射返回 `MAP_FAILED`，记录 `artifacts/runs/batch-20261102T030829-96acab04`，结果 PASSED。运行器的 `resource_limit` 终止原因来自 `SIGXCPU`。

同一用户命名空间映射之后，`CTR-005` 建立 UTS 命名空间并把主机名设为 `syslab-ns`，命名空间外的 `gethostname` 保持原值，记录 `artifacts/runs/batch-20261102T031535-0afa3d29`，结果 PASSED。`CTR-006` 建立网络命名空间后，`SIOCGIFINDEX` 能索引 `lo`，不能索引进入之前由 `SIOCGIFCONF` 选中的那块非 `lo` 网卡；原命名空间仍能索引它。`/sys/class/net` 在这次检查里仍列出原来的接口，实验没有读取该目录。记录 `artifacts/runs/batch-20261102T031543-06a6f56a`，结果 PASSED。

子进程的管道用 `pipe2(O_CLOEXEC)` 创建。子进程在 `exec` 前从描述符 3 关闭到 `sysconf(_SC_OPEN_MAX)`。本机该值是 262144，这段循环大约 145 毫秒。没有调用 `close_range`。父进程打不开标准输出或标准错误文件时，`CommandResult` 记下错误，内存中的管道内容仍然保留。超时后先 `SIGTERM`，约 200 毫秒后 `SIGKILL`，再过约 200 毫秒若管道仍开着或子进程尚未回收，就关闭读端并返回，不再阻塞 `waitpid`。输出超限引起的结束仍记为 `output_limit`。

2026-11-02，在 Clang 12.0.1 上用 `make asan` 编译并运行 `build/asan/syslab_tests`。fiber 在 `swapcontext` 前调用 `__sanitizer_start_switch_fiber`，进入 fiber 入口和切回之后调用 `__sanitizer_finish_switch_fiber`。TSan 接口 `__tsan_create_fiber`、`__tsan_switch_to_fiber` 和 `__tsan_destroy_fiber` 只在 `__has_feature(thread_sanitizer)` 下编译。2026-11-02 用 `build/tsan/syslab_tests` 运行单元测试时，`fiber_coroutine_and_steal` 在 `__tsan_func_entry` 发生 SEGV，进程状态为 66，没有打印数据竞争报告。该测试之前的单元测试单独运行时通过。这次没有把 SEGV 改成通过。ASan 仍打印 `makecontext/swapcontext` 支持不完整的提示。`ASAN_OPTIONS=detect_leaks=0` 时断言通过，进程状态为 0。默认 LeakSanitizer 报告 `LockFreeStack<int>::push`（`include/syslab/atomic/lockfree_stack.hpp:49`）和 `test_epoch`（`tests/unit/test_main.cpp:388`）的未回收对象，合计 516 字节、33 次分配，进程状态为 1。`CMakePresets.json` 的 version 是 2。`cmake --list-presets` 在本机 CMake 3.20.2 上可以列出 asan 和 tsan 预设。实验的编译参数包含 `-fsanitize` 时，会话把该次运行的标准错误记为 `SanitizerReport`。清单要求 `sanitizer_report` 而编译参数没有 `-fsanitize` 时，这条观测的 `complete` 为 false。上述实验路径没有在本机用 Sanitizer 编译运行过。
