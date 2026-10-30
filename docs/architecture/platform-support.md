# 平台支持

manifest 的 `platform.os` 必须是 `linux`。`architectures` 只接受 `x86_64` 和 `aarch64`。当前开发主机是 x86-64，AArch64 源码路径有条件编译，但没有在 AArch64 机器或 QEMU 上运行。

CPU-004 的 `abi_sum.S` 按架构选择寄存器：x86-64 使用 System V 的前六个整数参数寄存器，AArch64 使用 PCS 的 x0 到 x5。数值检查是返回值 21，不是某条机器码。

协程需要 Clang 在没有 `<coroutine>` 时找到 polyfill 里的 `std::experimental::coroutine_traits`。有标准 `<coroutine>` 时 CMake 不再加入该包含路径。

`libatomic.so` 在本开发主机上不可用。无锁结构的 `is_lock_free()` 使用 `__atomic_always_lock_free`，不调用会引入 `libatomic` 的 `std::atomic::is_lock_free`。指针宽度或 `size_t` 不是始终无锁时，相关实验返回 unsupported。

环境快照中的编译器版本字段固定为 unavailable，原因是版本由工具链探测和 `build.json` 分别记录，避免两处互相覆盖成空字符串。
