#include "experiments/common/check.hpp"
#include "syslab/atomic/spsc_ring.hpp"
#include <thread>
// 原理：SpscRing<int, 1024> 的容量为 1024。一个生产者写入 0 到 9999，消费者依次得到这些值。
int main() {
  syslab::SpscRing<int, 1024> ring;
  if (!ring.is_lock_free()) {
    syslab_step("索引原子不是始终无锁，本节停止。");
    return syslab_unsupported("index-atomic-not-lock-free");
  }
  std::thread producer([&]() {
    for (int value = 0; value < 10000; ++value) while (!ring.push(value)) {}
  });
  int expected = 0;
  int value = 0;
  while (expected < 10000) {
    if (ring.pop(value)) {
      if (value != expected) {
    syslab_step("执行顺序或交出顺序与预期不一致。");
    return syslab_fail("order");
  }
      ++expected;
    }
  }
  producer.join();
  syslab_note("OBSERVED", "one producer owns the tail and one consumer owns the head");
    syslab_step("消费者依次得到 0 到 9999。");
  return syslab_pass();
}
