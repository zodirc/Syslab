#pragma once

#include "syslab/synchronization/futex.hpp"
#include "syslab/synchronization/mutex.hpp"

#include <atomic>
#include <cstdint>

namespace syslab {

/**
 * @brief Futex condition variable associated with a Mutex.
 * 与 Mutex 配合的 futex 条件变量。
 *
 * wait loads a sequence, unlocks the mutex, then FUTEX_WAITs on that sequence.
 * notify increments the sequence with release and wakes waiters. If the
 * increment happens before the wait, the futex word no longer matches and the
 * wait returns. Waiters must recheck their predicate after locking the mutex
 * again. Spurious wakeups are allowed. notify may run without the mutex, but
 * the predicate data must still be published by the mutex or by an atomic
 * release that the waiter observes. This is a teaching condition variable.
 * wait 读取序列号，解锁互斥量，然后在该序列号上 FUTEX_WAIT。notify 用 release 增加序列号并唤醒等待者。若增加发生在等待之前，futex 字不再匹配，等待会返回。等待者重新锁上互斥量后必须复查谓词。允许伪唤醒。notify 可以不持有互斥量，但谓词数据仍须由互斥量或等待者能观察到的原子 release 发布。这是教学条件变量。
 */
class ConditionVariable {
 public:
  ConditionVariable() = default;
  ConditionVariable(const ConditionVariable&) = delete;
  ConditionVariable& operator=(const ConditionVariable&) = delete;

  /**
   * @brief Waits until a notification is observed.
   * 一直等到观察到通知。
   * @param mutex Mutex locked by the caller. It is re-locked before return.
   * 调用者已锁住的互斥量。返回前会重新锁上。
   */
  void wait(Mutex& mutex) {
    const std::uint32_t observed = sequence_.load(std::memory_order_relaxed);
    mutex.unlock();
    futex_wait(&sequence_, observed);
    mutex.lock();
  }

  /**
   * @brief Waits until the predicate returns true.
   * 一直等到谓词返回真。
   * @tparam Predicate Callable invoked with the mutex held.
   * 在持有互斥量时调用的可调用对象。
   * @param mutex Mutex locked by the caller.
   * 调用者已锁住的互斥量。
   * @param predicate Condition rechecked after every wakeup.
   * 每次唤醒后重新检查的条件。
   */
  template <typename Predicate>
  void wait(Mutex& mutex, Predicate predicate) {
    while (!predicate()) {
      wait(mutex);
    }
  }

  /**
   * @brief Wakes one waiter.
   * 唤醒一个等待者。
   */
  void notify_one() {
    sequence_.fetch_add(1, std::memory_order_release);
    futex_wake(&sequence_, 1);
  }

  /**
   * @brief Wakes every current waiter.
   * 唤醒当前全部等待者。
   */
  void notify_all() {
    sequence_.fetch_add(1, std::memory_order_release);
    futex_wake(&sequence_, 1 << 30);
  }

 private:
  std::atomic<std::uint32_t> sequence_{0};
};

}  // namespace syslab
