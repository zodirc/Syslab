#pragma once

#include "syslab/synchronization/futex.hpp"

#include <atomic>
#include <cstdint>

namespace syslab {

/**
 * @brief Counting semaphore implemented with an atomic counter and futex.
 * 用原子计数和 futex 实现的计数信号量。
 *
 * acquire waits while the counter is zero, then decrements it with acquire.
 * release increments the counter with release and wakes one waiter. A release
 * that happens after the load of zero and before the wait makes the futex word
 * differ from zero, so the wait returns and the loop retries. The counter must
 * not overflow uint32_t. Destroy the semaphore only when no thread is waiting.
 * This is a teaching semaphore.
 * acquire 在计数为零时等待，然后用 acquire 递减。release 用 release 递增并唤醒一个等待者。若 release 发生在读到零之后、等待之前，futex 字不再是零，等待会返回并让循环重试。计数不得溢出 uint32_t。没有线程等待时才能销毁。这是教学信号量。
 */
class Semaphore {
 public:
  /**
   * @brief Creates a semaphore with the given permit count.
   * 用给定的许可数创建信号量。
   * @param permits Initial permits.
   * 初始许可数。
   */
  explicit Semaphore(std::uint32_t permits) : count_(permits) {}
  Semaphore(const Semaphore&) = delete;
  Semaphore& operator=(const Semaphore&) = delete;

  /**
   * @brief Takes one permit, waiting while the count is zero.
   * 取得一个许可，计数为零时等待。
   */
  void acquire() {
    while (true) {
      std::uint32_t value = count_.load(std::memory_order_acquire);
      while (value > 0) {
        if (count_.compare_exchange_weak(value, value - 1, std::memory_order_acquire,
                                         std::memory_order_relaxed)) {
          return;
        }
      }
      futex_wait(&count_, 0);
    }
  }

  /**
   * @brief Returns one permit and wakes one waiter.
   * 归还一个许可并唤醒一个等待者。
   */
  void release() {
    count_.fetch_add(1, std::memory_order_release);
    futex_wake(&count_, 1);
  }

 private:
  std::atomic<std::uint32_t> count_;
};

}  // namespace syslab
