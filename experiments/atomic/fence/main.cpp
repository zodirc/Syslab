#include "experiments/common/check.hpp"
#include <atomic>
#include <thread>
// 应用：已经使用 relaxed 原子和栅栏，旗标为真时载荷仍是旧值。
// 原理：写线程在 relaxed 存储载荷之后执行 release fence，再 relaxed 存储旗标。读线程看到旗标后执行 acquire fence，再读取载荷。重复 200 次，载荷为 42。这里核对的是 C++ 的 atomic_thread_fence。
int main() {
  for (int trial = 0; trial < 200; ++trial) {
    std::atomic<int> payload{0};
    std::atomic<int> flag{0};
    std::thread writer([&]() {
      payload.store(42, std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_release);
      flag.store(1, std::memory_order_relaxed);
    });
    while (flag.load(std::memory_order_relaxed) == 0) {}
    std::atomic_thread_fence(std::memory_order_acquire);
    const int seen = payload.load(std::memory_order_relaxed);
    writer.join();
    if (seen != 42) {
    syslab_step("acquire fence 之后的载荷不是 42。");
    return syslab_fail("fence");
  }
  }
  syslab_note("LANG", "atomic_thread_fence release paired with acquire and the payload was 42");
    syslab_step("200 次试验中，acquire fence 之后读到的载荷都是 42。");
  return syslab_pass();
}
