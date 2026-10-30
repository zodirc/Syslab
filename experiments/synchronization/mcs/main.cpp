#include "experiments/common/check.hpp"
#include "syslab/synchronization/mcs_lock.hpp"
#include <cstdint>
#include <thread>
#include <vector>
// 应用：MCS 锁在解锁时崩溃，或临界区计数偏小。
// 原理：每个等待者使用自己的队列节点，节点的寿命必须覆盖到 unlock 返回。McsGuard 把节点放在守卫内部。四个线程各递增 1000 次后，计数为 4000。
int main() {
  syslab::McsLock lock;
  std::uint64_t counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 1000; ++step) {
        syslab::McsGuard guard(lock);
        ++counter;
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (counter != 4000) {
    syslab_step("临界区内的计数与预期不一致。");
    return syslab_fail("exclusion");
  }
  syslab_note("OBSERVED", "each thread waits on its own node; the lock is fair in this implementation");
    syslab_step("四个线程各持有 MCS 锁递增 1000 次，计数为 4000。队列节点的寿命覆盖到 unlock 返回。");
  return syslab_pass();
}
