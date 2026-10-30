#include "experiments/common/check.hpp"
#include <atomic>
#include <thread>
#include <vector>
// 应用：计数总和正确，但同一时刻发布的其他普通数据仍然是旧值。
// 原理：relaxed fetch_add 保证读改写的原子性。四线程共 40000 次之后计数为 40000。relaxed 不建立与其他内存的 happens-before。
int main() {
  std::atomic<int> counter{0};
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 10000; ++step) counter.fetch_add(1, std::memory_order_relaxed);
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (counter.load(std::memory_order_relaxed) != 40000) {
    syslab_step("relaxed 递增之后的总和不正确。");
    return syslab_fail("total");
  }
  syslab_note("LANG", "relaxed RMW is atomic; it does not establish happens-before with a payload");
    syslab_step("40000 次 relaxed fetch_add 之后，计数为 40000。这次总和只证明原子性。");
  return syslab_pass();
}
