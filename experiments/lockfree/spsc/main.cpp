#include "experiments/common/check.hpp"
#include "syslab/atomic/spsc_ring.hpp"
#include <thread>
// 应用：单生产者单消费者队列交出的整数乱序或出现缺口。
// 原理：容量为 2 的幂。一个生产者拥有尾，一个消费者拥有头。消费者依次得到 0 到 9999。第二个生产者或第二个消费者不属于这个算法。
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
