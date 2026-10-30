#pragma once

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>

namespace syslab {

/**
 * @brief Waits if the futex word still contains the expected value.
 * 若 futex 字仍等于期望值则等待。
 *
 * This is the Linux FUTEX_WAIT operation. It is an OS blocking primitive, not
 * a C++ atomic fence and not an ISA memory barrier. Spurious wakeups are
 * possible. The atomic object must remain alive for the duration of the wait.
 * 这是 Linux 的 FUTEX_WAIT。它是操作系统阻塞原语，不是 C++ 原子 fence，也不是 ISA 内存屏障。可能发生伪唤醒。等待期间原子对象必须保持存活。
 *
 * @param word Futex word.
 * futex 字。
 * @param expected Value observed by the caller before waiting.
 * 调用者在等待前观察到的值。
 * @return The syscall result. Zero means a wake. EAGAIN means the word changed.
 * 系统调用结果。零表示被唤醒。EAGAIN 表示字已经改变。
 */
inline long futex_wait(std::atomic<std::uint32_t>* word, std::uint32_t expected) {
  return syscall(SYS_futex, reinterpret_cast<std::uint32_t*>(word), FUTEX_WAIT, expected, nullptr, nullptr, 0);
}

/**
 * @brief Wakes threads blocked in futex_wait on the same word.
 * 唤醒在同一字上 futex_wait 的线程。
 * @param word Futex word.
 * futex 字。
 * @param count Maximum number of waiters to wake. Use a large value for all.
 * 最多唤醒的等待者数量。唤醒全部时使用一个大值。
 * @return Number of woken waiters, or -1 on failure.
 * 被唤醒的等待者数量，失败时为 -1。
 */
inline long futex_wake(std::atomic<std::uint32_t>* word, int count) {
  return syscall(SYS_futex, reinterpret_cast<std::uint32_t*>(word), FUTEX_WAKE, count, nullptr, nullptr, 0);
}

/**
 * @brief Returns the kernel thread identifier of the caller.
 * 返回调用者的内核线程标识。
 * @return Linux tid from gettid.
 * 来自 gettid 的 Linux tid。
 */
inline int current_tid() { return static_cast<int>(syscall(SYS_gettid)); }

}  // namespace syslab
