#include "experiments/common/check.hpp"
#include "syslab/synchronization/mutex.hpp"
#include "syslab/synchronization/spinlock.hpp"
#include <cstdint>
#include <thread>
#include <vector>
// 应用：自旋锁保护的计数小于线程数乘以每次循环的递增次数。
// 原理：四个线程在同一把测试并设置锁内各递增 2000 次。临界区结束时计数为 8000。这把锁不保证先到先服务。
int main() {
  syslab::SpinLock lock;
  std::uint64_t counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 2000; ++step) {
        syslab::LockGuard<syslab::SpinLock> guard(lock);
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
  syslab_note("OBSERVED", "test-and-set exclusion; the lock is not fair");
    syslab_step("四个线程在测试并设置锁内各递增 2000 次，计数为 8000。");
  return syslab_pass();
}
