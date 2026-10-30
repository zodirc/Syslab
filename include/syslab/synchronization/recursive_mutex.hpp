#pragma once

#include "syslab/core/error.hpp"
#include "syslab/synchronization/futex.hpp"
#include "syslab/synchronization/mutex.hpp"

#include <atomic>

namespace syslab {

/**
 * @brief Mutex that the owning thread may acquire more than once.
 * 拥有者线程可以多次获取的互斥量。
 *
 * The first acquisition takes the inner futex mutex and publishes the owner
 * tid with release. Later acquisitions by that tid only increment a depth
 * counter. The depth counter is touched only by the owner, so it does not need
 * its own atomic. Unlock of the last depth clears the owner with release and
 * unlocks the inner mutex. The mutex is not fair beyond the inner mutex.
 * Destroy it only at depth zero. It is not signal-safe. This is a teaching mutex.
 * 第一次获取拿到内部 futex 互斥量，并用 release 公布拥有者 tid。同一 tid 的后续获取只增加深度计数。深度只由拥有者修改，因此不需要自己的原子。最后一层解锁用 release 清除拥有者并解开内部互斥量。公平性不超过内部互斥量。只在深度为零时销毁。它不是信号安全的。这是教学互斥量。
 */
class RecursiveMutex {
 public:
  RecursiveMutex() = default;
  RecursiveMutex(const RecursiveMutex&) = delete;
  RecursiveMutex& operator=(const RecursiveMutex&) = delete;

  /**
   * @brief Acquires the mutex, or increments the depth when the caller already owns it.
   * 获取互斥量；调用者已拥有时增加深度。
   */
  void lock() {
    const int self = current_tid();
    if (owner_.load(std::memory_order_acquire) == self) {
      ++depth_;
      return;
    }
    mutex_.lock();
    owner_.store(self, std::memory_order_release);
    depth_ = 1;
  }

  /**
   * @brief Decrements the depth and releases the inner mutex at depth zero.
   * 减少深度，深度为零时释放内部互斥量。
   * @throws Error Logic when the caller is not the owner.
   * 调用者不是拥有者时抛出 Error Logic。
   */
  void unlock() {
    const int self = current_tid();
    if (owner_.load(std::memory_order_relaxed) != self || depth_ <= 0) {
      throw Error(Error::Code::Logic, "recursive mutex unlock by a non-owner");
    }
    if (--depth_ == 0) {
      owner_.store(0, std::memory_order_release);
      mutex_.unlock();
    }
  }

 private:
  Mutex mutex_;
  std::atomic<int> owner_{0};
  int depth_ = 0;
};

}  // namespace syslab
