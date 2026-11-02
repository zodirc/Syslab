#include "experiments/common/check.hpp"
#include "syslab/synchronization/semaphore.hpp"
#include <atomic>
#include <thread>
#include <vector>
// 应用：同时使用某项资源的线程数超过了设定的许可数。
// 原理：信号量初值为 2。四个线程各获取并释放 40 次。观察到的最大同时持有数不超过 2。调度没有让两个持有者重叠时，这个最大值可以小于 2。超过 2 时检查失败。
int main() {
  syslab::Semaphore semaphore(2);
  std::atomic<int> active{0};
  std::atomic<int> maximum{0};
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 40; ++step) {
        semaphore.acquire();
        const int now = active.fetch_add(1) + 1;
        int seen = maximum.load();
        while (now > seen && !maximum.compare_exchange_weak(seen, now)) {}
        active.fetch_sub(1);
        semaphore.release();
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (maximum.load() > 2) {
    syslab_step("同时持有的许可数超过了初值。");
    return syslab_fail("permits");
  }
  syslab_note("OBSERVED", "the permit count is an atomic; waiters sleep with futex when it is zero");
    syslab_step("同时持有的许可数不超过初值 2。");
  return syslab_pass();
}
