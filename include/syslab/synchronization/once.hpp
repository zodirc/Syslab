#pragma once

#include "syslab/synchronization/futex.hpp"

#include <atomic>
#include <cstdint>
#include <exception>
#include <utility>

namespace syslab {

/**
 * @brief One-time initialization flag.
 * 一次性初始化标志。
 *
 * State 0 is not started, 1 is running, and 2 is complete. The winner of the
 * compare-exchange runs the function. Other threads futex-wait while the state
 * is 1. Completion is stored with release and waited with acquire, so the
 * effects of the function happen-before later callers observe completion. If
 * the function throws, the state returns to 0 and waiters retry. Destroy the
 * flag only after call_once has completed or no thread is inside it.
 * 状态 0 是未开始，1 是执行中，2 是已完成。比较交换的胜者执行函数。其他线程在状态为 1 时 futex 等待。完成用 release 存储，等待用 acquire，因此函数的效果 happens-before 后来的调用者观察到完成。函数抛出异常时状态回到 0，等待者会重试。只在 call_once 已完成或没有线程停在其中时销毁。
 */
class OnceFlag {
 public:
  OnceFlag() = default;
  OnceFlag(const OnceFlag&) = delete;
  OnceFlag& operator=(const OnceFlag&) = delete;

  /**
   * @brief Runs the function once.
   * 把函数运行一次。
   * @tparam Function Callable with no arguments.
   * 无参数的可调用对象。
   * @param function Initialization function. It may throw.
   * 初始化函数。它可以抛出异常。
   * @throws The exception thrown by function. Another caller may then retry.
   * 函数抛出的异常。另一个调用者随后可以重试。
   */
  template <typename Function>
  void call_once(Function&& function) {
    std::uint32_t state = state_.load(std::memory_order_acquire);
    if (state == 2) {
      return;
    }
    std::uint32_t expected = 0;
    if (state_.compare_exchange_strong(expected, 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
      try {
        std::forward<Function>(function)();
      } catch (...) {
        state_.store(0, std::memory_order_release);
        futex_wake(&state_, 1 << 30);
        throw;
      }
      state_.store(2, std::memory_order_release);
      futex_wake(&state_, 1 << 30);
      return;
    }
    while (true) {
      state = state_.load(std::memory_order_acquire);
      if (state == 2) {
        return;
      }
      if (state == 0) {
        call_once(std::forward<Function>(function));
        return;
      }
      futex_wait(&state_, 1);
    }
  }

 private:
  std::atomic<std::uint32_t> state_{0};
};

}  // namespace syslab
