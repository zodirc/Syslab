#include "experiments/common/check.hpp"
#include "syslab/synchronization/condition_variable.hpp"
#include "syslab/synchronization/mutex.hpp"
#include <thread>
// 应用：条件变量上的等待者被唤醒后继续使用旧数据，或者通知发出时等待者尚未进入等待。
// 原理：等待者在持锁状态下反复检查谓词 value == 7。通知方在持锁时把 value 设为 7 再通知。等待者返回时谓词已经成立。伪唤醒由谓词复查挡住。本节不证明 futex 进入了睡眠。
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
  syslab_note("OS", "the waiter ran after the predicate became true; the kernel wait queue is not identified");
    syslab_step("等待线程返回时，持锁发布的 value 已经是 7。");
  return syslab_pass();
}
