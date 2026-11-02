#include "experiments/common/check.hpp"
#include "syslab/synchronization/mcs_lock.hpp"

#include <cstdint>
#include <thread>
#include <vector>

// 应用：MCS 锁在解锁时崩溃，或临界区计数偏小。
// 原理：同一节点在 unlock 返回后可以再次加锁。四个线程用 McsGuard 各递增 1000 次，计数为 4000。本节不构造 unlock 返回前销毁节点的用例。

int main() {
  syslab::McsLock lock;
  syslab::McsNode node;
  lock.lock(node);
  lock.unlock(node);
  lock.lock(node);
  lock.unlock(node);

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
  for (std::thread& thread : threads) {
    thread.join();
  }
  if (counter != 4000) {
    syslab_step("临界区内的计数与预期不一致。");
    return syslab_fail("exclusion");
  }
  syslab_note("OBSERVED", "the same node locked again after unlock returned; the guarded count is 4000");
  syslab_step("同一节点在 unlock 返回后再次加锁成功。四个线程各递增 1000 次，计数为 4000。");
  return syslab_pass();
}
