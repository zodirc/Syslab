#pragma once

namespace syslab {

/**
 * @brief Issues an ISA store-ordering barrier when the host defines one.
 * 主机有定义时发出 ISA 存储排序屏障。
 *
 * On x86-64 this is sfence. On AArch64 this is dmb ishst. The function does
 * not create a C++ synchronizes-with edge.
 * x86-64 上是 sfence。AArch64 上是 dmb ishst。本函数不建立 C++ 的 synchronizes-with 边。
 */
inline void isa_store_barrier() noexcept {
#if defined(__x86_64__)
  asm volatile("sfence" ::: "memory");
#elif defined(__aarch64__)
  asm volatile("dmb ishst" ::: "memory");
#else
  asm volatile("" ::: "memory");
#endif
}

/**
 * @brief Issues an ISA load-ordering barrier when the host defines one.
 * 主机有定义时发出 ISA 加载排序屏障。
 *
 * On x86-64 this is lfence. On AArch64 this is dmb ishld.
 * x86-64 上是 lfence。AArch64 上是 dmb ishld。
 */
inline void isa_load_barrier() noexcept {
#if defined(__x86_64__)
  asm volatile("lfence" ::: "memory");
#elif defined(__aarch64__)
  asm volatile("dmb ishld" ::: "memory");
#else
  asm volatile("" ::: "memory");
#endif
}

/**
 * @brief Issues a full ISA memory barrier.
 * 发出完整的 ISA 内存屏障。
 *
 * On x86-64 this is mfence. On AArch64 this is dmb ish. It is not a substitute
 * for std::atomic_thread_fence.
 * x86-64 上是 mfence。AArch64 上是 dmb ish。它不能代替 std::atomic_thread_fence。
 */
inline void isa_full_barrier() noexcept {
#if defined(__x86_64__)
  asm volatile("mfence" ::: "memory");
#elif defined(__aarch64__)
  asm volatile("dmb ish" ::: "memory");
#else
  asm volatile("" ::: "memory");
#endif
}

/**
 * @brief Returns the mnemonic emitted by isa_full_barrier on this host.
 * 返回本机 isa_full_barrier 发出的助记符。
 * @return Instruction mnemonic, or "compiler-only" when no ISA barrier is emitted.
 * 指令助记符。没有发出 ISA 屏障时为 "compiler-only"。
 */
inline const char* isa_full_barrier_mnemonic() noexcept {
#if defined(__x86_64__)
  return "mfence";
#elif defined(__aarch64__)
  return "dmb ish";
#else
  return "compiler-only";
#endif
}

}  // namespace syslab
