#include "experiments/common/check.hpp"
#include <atomic>
#include <thread>
// 应用：旗标已经为真，读到的载荷仍是写入旗标之前的旧值。
// 原理：写线程用 relaxed 写入载荷 42，再用 release 写入旗标 1。读线程用 acquire 看到旗标为 1 之后，载荷为 42。重复 200 次。这是语言内存模型的要求。
int main() {
  for (int trial = 0; trial < 200; ++trial) {
    std::atomic<int> payload{0};
    std::atomic<int> flag{0};
    std::thread writer([&]() {
      payload.store(42, std::memory_order_relaxed);
      flag.store(1, std::memory_order_release);
    });
    int seen = 0;
    while (flag.load(std::memory_order_acquire) == 0) {}
    seen = payload.load(std::memory_order_relaxed);
    writer.join();
    if (seen != 42) {
    syslab_step("旗标可见之后，载荷不是 42。");
    return syslab_fail("payload");
  }
  }
  syslab_note("LANG", "release synchronizes with acquire, so the payload store happens-before the payload load");
    syslab_step("200 次试验中，acquire 看到旗标为 1 之后，载荷都是 42。");
  return syslab_pass();
}
