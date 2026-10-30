#include "experiments/common/check.hpp"
#include "syslab/synchronization/mutex.hpp"
#include <thread>
#include <vector>
// 应用：多个线程共同维护一个计数，业务上需要这个计数等于全部递增之和。
// 原理：四个线程在互斥量保护下各递增 1000 次，计数为 4000。未同步的普通整数递增是数据竞争。本节不运行那条路径，因此一次未崩溃不能证明不存在数据竞争。
int main() {
  syslab::Mutex mutex;
  int counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 1000; ++step) {
        syslab::LockGuard<syslab::Mutex> guard(mutex);
        ++counter;
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (counter != 4000) {
    syslab_step("互斥保护下的计数与递增之和不一致。");
    return syslab_fail("counter");
  }
  syslab_note("LANG", "an unsynchronized increment of a non-atomic int would be a data race and is not executed");
    syslab_step("四线程在互斥量保护下各递增 1000 次，计数为 4000。");
  return syslab_pass();
}
