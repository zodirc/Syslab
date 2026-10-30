#include "experiments/common/check.hpp"
#include "syslab/synchronization/barrier.hpp"
#include <atomic>
#include <thread>
#include <vector>
// 应用：屏障返回之后，仍有线程看到尚未到达的参与者。
// 原理：四个参与者都到达后，每个线程看到的到达计数为 4。到达计数用原子发布。等待的是代数。
int main() {
  syslab::Barrier barrier(4);
  std::atomic<int> arrived{0};
  std::atomic<int> failures{0};
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      arrived.fetch_add(1);
      barrier.arrive_and_wait();
      if (arrived.load() != 4) failures.fetch_add(1);
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (failures.load() != 0) {
    syslab_step("屏障返回后仍有线程看到未到齐的计数。");
    return syslab_fail("generation");
  }
  syslab_note("OBSERVED", "the last arrival publishes the new generation with release");
    syslab_step("屏障返回后，每个参与者看到的到达计数都是 4。");
  return syslab_pass();
}
