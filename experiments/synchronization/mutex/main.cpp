#include "experiments/common/check.hpp"
#include "syslab/synchronization/mutex.hpp"
#include <cstdint>
#include <thread>
#include <vector>
// 应用：互斥量保护的计数偏小，或等待行为与锁的状态对不上。
// 原理：这把 futex 互斥量用 0、1、2 记录状态。四个线程各递增 2000 次，计数为 8000。
int main() {
  syslab::Mutex lock;
  std::uint64_t counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 2000; ++step) {
        syslab::LockGuard<syslab::Mutex> guard(lock);
        ++counter;
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (counter != 8000) {
    syslab_step("临界区内的计数与预期不一致。");
    return syslab_fail("exclusion");
  }
  syslab_metric("counter", static_cast<long long>(counter));
  syslab_note("OBSERVED", "four threads incremented inside the futex mutex and the counter is 8000");
    syslab_step("四个线程在 futex 互斥量内各递增 2000 次，计数为 8000。");
  return syslab_pass();
}
