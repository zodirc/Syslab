#pragma once

#include "syslab/platform/cpu.hpp"

#include <atomic>
#include <cstdint>

namespace syslab {

/**
 * @brief Test-and-set spin lock.
 * 测试并设置自旋锁。
 *
 * State is 0 when unlocked and 1 when locked. Acquisition is a successful
 * exchange of 1 with acquire ordering. Release stores 0 with release ordering.
 * The lock is not fair, not reentrant, and must not be destroyed while locked
 * or while another thread is spinning. A thread that is cancelled inside the
 * critical section leaves the lock held. This is a teaching lock.
 * 未锁为 0，已锁为 1。获取是用 acquire 顺序成功把值换成 1。释放用 release 顺序存入 0。锁不公平、不可重入，锁定中或仍有线程自旋时不得销毁。线程在临界区内被取消会让锁保持持有。这是教学锁。
 */
class SpinLock {
 public:
  SpinLock() = default;
  SpinLock(const SpinLock&) = delete;
  SpinLock& operator=(const SpinLock&) = delete;

  /**
   * @brief Acquires the lock, spinning until it is observed unlocked.
   * 获取锁，一直自旋到观察到未锁。
   */
  void lock() noexcept {
    while (state_.exchange(1, std::memory_order_acquire) != 0) {
      while (state_.load(std::memory_order_relaxed) != 0) {
        cpu_relax();
      }
    }
  }

  /**
   * @brief Releases the lock.
   * 释放锁。
   *
   * Unlocking a lock that is not held by the caller is undefined for the
   * teaching invariant and is reported by a store that can wake a later locker
   * incorrectly. Callers must pair lock and unlock on the same thread.
   * 调用者未持有锁就解锁，对教学不变量是未定义的，并且一次存储可能错误地唤醒后来的加锁者。调用者必须在同一线程上成对 lock 和 unlock。
   */
  void unlock() noexcept { state_.store(0, std::memory_order_release); }

  /**
   * @brief Attempts to acquire the lock once.
   * 尝试获取锁一次。
   * @return True when this call acquired the lock.
   * 这次调用取得锁时为真。
   */
  bool try_lock() noexcept {
    std::uint32_t expected = 0;
    return state_.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
  }

 private:
  std::atomic<std::uint32_t> state_{0};
};

}  // namespace syslab
