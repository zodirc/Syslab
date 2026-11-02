#include "experiments/common/check.hpp"
#include <atomic>
#include <thread>
#include <vector>
// 应用：计数总和正确，但同一时刻发布的其他普通数据仍然是旧值。
// 原理：四个线程各执行 10000 次 memory_order_relaxed 的 fetch_add。计数为 40000。
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
  syslab_note("LANG", "relaxed fetch_add completed 40000 times and the counter is 40000");
    syslab_step("40000 次 relaxed fetch_add 之后，计数为 40000。");
  return syslab_pass();
}
