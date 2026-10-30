#pragma once

#include "syslab/core/error.hpp"
#include "syslab/synchronization/futex.hpp"

#include <atomic>
#include <cstdint>

namespace syslab {

/**
 * @brief Futex mutex.
 * 基于 futex 的互斥量。
 *
 * State 0 is unlocked, 1 is locked with no published waiters, and 2 is locked
 * with waiters. The fast path is an acquire compare-exchange from 0 to 1.
 * The slow path exchanges in 2 and waits with FUTEX_WAIT while the word is 2.
 * Unlock exchanges in 0 with release and wakes one waiter when the previous
 * state was 2. The lock is not fair and not reentrant. Destroy it only when
 * unlocked and no thread is inside lock. A thread that exits while holding the
 * lock leaves it held. This is a teaching mutex.
 * 状态 0 是未锁，1 是已锁且没有公布等待者，2 是已锁且有等待者。快路径是从 0 到 1 的 acquire 比较交换。慢路径交换进 2，并在字为 2 时 FUTEX_WAIT。解锁用 release 交换进 0，前一状态为 2 时唤醒一个等待者。锁不公平、不可重入。只在未锁且没有线程停在 lock 内时销毁。持有锁的线程退出后锁仍被持有。这是教学互斥量。
 */
class Mutex {
 public:
  Mutex() = default;
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  /**
   * @brief Acquires the mutex, blocking through futex when the fast path fails.
   * 获取互斥量。快路径失败时通过 futex 阻塞。
   */
  void lock() {
    std::uint32_t expected = 0;
    if (state_.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
      return;
    }
    slow_path_.fetch_add(1, std::memory_order_relaxed);
    while (state_.exchange(2, std::memory_order_acquire) != 0) {
      futex_wait(&state_, 2);
    }
  }

  /**
   * @brief Releases the mutex and wakes one waiter when the slow path was published.
   * 释放互斥量。慢路径已公布时唤醒一个等待者。
   * @throws Error Logic when the mutex is not locked.
   * 互斥量未锁时抛出 Error Logic。
   */
  void unlock() {
    const std::uint32_t previous = state_.exchange(0, std::memory_order_release);
    if (previous == 0) {
      throw Error(Error::Code::Logic, "mutex unlock of an unlocked mutex");
    }
    if (previous == 2) {
      futex_wake(&state_, 1);
    }
  }

  /**
   * @brief Attempts to acquire the mutex without waiting.
   * 尝试获取互斥量，不等待。
   * @return True when this call acquired the mutex.
   * 这次调用取得互斥量时为真。
   */
  bool try_lock() {
    std::uint32_t expected = 0;
    return state_.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
  }

  /**
   * @brief Returns how many times lock entered the futex path.
   * 返回 lock 进入 futex 路径的次数。
   * @return Slow-path entry count. This is a teaching counter, not a proof that a wait occurred.
   * 慢路径进入次数。这是教学计数，不能证明发生过等待。
   */
  std::uint64_t slow_path_count() const { return slow_path_.load(std::memory_order_relaxed); }

 private:
  std::atomic<std::uint32_t> state_{0};
  std::atomic<std::uint64_t> slow_path_{0};
};

/**
 * @brief Releases a lock from its destructor.
 * 在析构时释放锁。
 * @tparam Lock Type with lock and unlock methods.
 * 带有 lock 和 unlock 方法的类型。
 */
template <typename Lock>
class LockGuard {
 public:
  /**
   * @brief Acquires the lock.
   * 获取锁。
   * @param lock Lock that remains owned by the caller.
   * 锁仍由调用者拥有。
   */
  explicit LockGuard(Lock& lock) : lock_(lock) { lock_.lock(); }
  ~LockGuard() { lock_.unlock(); }
  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;

 private:
  Lock& lock_;
};

}  // namespace syslab
