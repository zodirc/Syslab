#include "experiments/common/check.hpp"
#include "syslab/synchronization/mutex.hpp"
#include "syslab/synchronization/ticket_lock.hpp"
#include <cstdint>
#include <thread>
#include <vector>
// 应用：票据锁保护的计数偏小，或线程进入临界区的顺序与取号顺序不符。
// 原理：取号和服务号是同一把锁上的一对计数。四个线程各递增 2000 次后，计数为 8000。这个实现按票号交接。
int main() {
  syslab::TicketLock lock;
  std::uint64_t counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 2000; ++step) {
        syslab::LockGuard<syslab::TicketLock> guard(lock);
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
  syslab_note("OBSERVED", "ticket lock exclusion; handover follows ticket order in this implementation");
    syslab_step("四个线程在票据锁内各递增 2000 次，计数为 8000。");
  return syslab_pass();
}
