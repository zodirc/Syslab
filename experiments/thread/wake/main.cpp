#include "experiments/common/check.hpp"
#include "syslab/synchronization/condition_variable.hpp"
#include "syslab/synchronization/mutex.hpp"
#include <thread>
// 原理：通知方持锁把 ready 写成 true 并调用 notify_one。等待者从 wait 返回后把 observed 写成 true。
int main() {
  syslab::Mutex mutex;
  syslab::ConditionVariable condition;
  bool ready = false;
  bool observed = false;
  std::thread waiter([&]() {
    syslab::LockGuard<syslab::Mutex> guard(mutex);
    condition.wait(mutex, [&]() { return ready; });
    observed = true;
  });
  {
    syslab::LockGuard<syslab::Mutex> guard(mutex);
    ready = true;
    condition.notify_one();
  }
  waiter.join();
  if (!observed) {
    syslab_step("谓词成立后等待者没有继续执行。");
    return syslab_fail("wake");
  }
  syslab_note("OS", "the waiter returned from wait and set observed after ready was stored under the mutex");
    syslab_step("等待线程从 wait 返回后，observed 为 true。");
  return syslab_pass();
}
