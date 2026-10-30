#pragma once

#include "syslab/core/error.hpp"
#include "syslab/synchronization/condition_variable.hpp"
#include "syslab/synchronization/mutex.hpp"

namespace syslab {

/**
 * @brief Writer-preferring read-write lock built on Mutex and ConditionVariable.
 * 用 Mutex 和 ConditionVariable 实现的写优先读写锁。
 *
 * Readers share the locked state. A writer excludes readers and other writers.
 * A waiting writer prevents new readers from entering, so writers are not
 * starved by a continuous reader stream. The lock is not reentrant. Destroy it
 * only when no thread holds it or waits. This is a teaching lock.
 * 读者共享锁定状态。写者排除读者和其他写者。已有写者等待时，新读者不能进入，因此连续的读者流不会饿死写者。锁不可重入。没有线程持有或等待时才能销毁。这是教学锁。
 */
class RwLock {
 public:
  RwLock() = default;
  RwLock(const RwLock&) = delete;
  RwLock& operator=(const RwLock&) = delete;

  /**
   * @brief Acquires exclusive ownership.
   * 获取独占所有权。
   */
  void lock() {
    LockGuard<Mutex> guard(mutex_);
    ++writers_waiting_;
    while (writer_active_ || readers_ > 0) {
      writer_cv_.wait(mutex_);
    }
    --writers_waiting_;
    writer_active_ = true;
  }

  /**
   * @brief Releases exclusive ownership.
   * 释放独占所有权。
   * @throws Error Logic when the caller does not hold write ownership.
   * 调用者没有写所有权时抛出 Error Logic。
   */
  void unlock() {
    LockGuard<Mutex> guard(mutex_);
    if (!writer_active_) {
      throw Error(Error::Code::Logic, "write unlock without an active writer");
    }
    writer_active_ = false;
    if (writers_waiting_ > 0) {
      writer_cv_.notify_one();
    } else {
      reader_cv_.notify_all();
    }
  }

  /**
   * @brief Acquires shared ownership.
   * 获取共享所有权。
   */
  void lock_shared() {
    LockGuard<Mutex> guard(mutex_);
    while (writer_active_ || writers_waiting_ > 0) {
      reader_cv_.wait(mutex_);
    }
    ++readers_;
  }

  /**
   * @brief Releases shared ownership.
   * 释放共享所有权。
   * @throws Error Logic when no reader is active.
   * 没有活跃读者时抛出 Error Logic。
   */
  void unlock_shared() {
    LockGuard<Mutex> guard(mutex_);
    if (readers_ <= 0) {
      throw Error(Error::Code::Logic, "read unlock without an active reader");
    }
    --readers_;
    if (readers_ == 0 && writers_waiting_ > 0) {
      writer_cv_.notify_one();
    }
  }

 private:
  Mutex mutex_;
  ConditionVariable reader_cv_;
  ConditionVariable writer_cv_;
  int readers_ = 0;
  int writers_waiting_ = 0;
  bool writer_active_ = false;
};

}  // namespace syslab
