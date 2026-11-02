#include "experiments/common/check.hpp"
#include "syslab/synchronization/condition_variable.hpp"
#include "syslab/synchronization/mutex.hpp"
#include <thread>
// 应用：等到了互斥量，读到的仍是通知之前的旧值。
// 原理：谓词 value == 7 在持锁期间发布。等待线程返回时 value 为 7。
int main() {
  syslab::Mutex mutex;
  syslab::ConditionVariable condition;
  int value = 0;
  std::thread worker([&]() {
    syslab::LockGuard<syslab::Mutex> guard(mutex);
    condition.wait(mutex, [&]() { return value == 7; });
  });
  {
    syslab::LockGuard<syslab::Mutex> guard(mutex);
    value = 7;
    condition.notify_one();
  }
  worker.join();
  if (value != 7) {
    syslab_step("等待者返回时谓词尚未成立。");
    return syslab_fail("predicate");
  }
  syslab_note("OBSERVED", "the sequence counter closes the lost-wakeup window in this condition variable");
    syslab_step("等待线程返回时，持锁发布的谓词 value 为 7。");
  return syslab_pass();
}
