#pragma once

#include "syslab/platform/cpu.hpp"

#include <atomic>
#include <cstdint>

namespace syslab {

/**
 * @brief FIFO ticket lock.
 * 先进先出的票锁。
 *
 * next_ticket hands out tickets with a relaxed fetch-add. serving_ publishes
 * the ticket that may enter, using release on unlock and acquire on the spin.
 * The lock is fair among threads that reach the fetch-add. It is not
 * reentrant. Destroy it only when unlocked and no thread is waiting. This is
 * a teaching lock.
 * next_ticket 用 relaxed fetch-add 发号。serving_ 发布可以进入的票号，解锁时 release，自旋时 acquire。对到达 fetch-add 的线程而言锁是公平的。它不可重入。只在未锁且没有线程等待时销毁。这是教学锁。
 */
class TicketLock {
 public:
  TicketLock() = default;
  TicketLock(const TicketLock&) = delete;
  TicketLock& operator=(const TicketLock&) = delete;

  /**
   * @brief Takes the next ticket and waits until it is served.
   * 取下一张票并等到它被服务。
   */
  void lock() noexcept {
    const std::uint32_t ticket = next_.fetch_add(1, std::memory_order_relaxed);
    while (serving_.load(std::memory_order_acquire) != ticket) {
      cpu_relax();
    }
  }

  /**
   * @brief Serves the next ticket.
   * 服务下一张票。
   *
   * The caller must hold the lock. The serving counter is incremented with
   * release so the next owner's acquire load observes the critical section.
   * 调用者必须持有锁。服务计数器用 release 增加，使下一位持有者的 acquire 加载能看到临界区。
   */
  void unlock() noexcept {
    const std::uint32_t now = serving_.load(std::memory_order_relaxed) + 1;
    serving_.store(now, std::memory_order_release);
  }

  /**
   * @brief Returns how many tickets have been handed out.
   * 返回已经发出的票数。
   * @return The value of the ticket counter. It increases when a caller reaches the fetch-add in lock.
   * 取号计数的当前值。调用者执行到 lock 里的 fetch-add 后增加。
   * @note A caller may hold a ticket and still be spinning. This call does not acquire the lock.
   * 调用者可能已经取到号，但仍在自旋。这次调用不获取锁。
   */
  std::uint32_t issued() const noexcept { return next_.load(std::memory_order_acquire); }

 private:
  std::atomic<std::uint32_t> next_{0};
  std::atomic<std::uint32_t> serving_{0};
};

}  // namespace syslab
