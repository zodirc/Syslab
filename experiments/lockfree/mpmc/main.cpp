#include "experiments/common/check.hpp"
#include "syslab/atomic/mpmc_queue.hpp"
#include <atomic>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
// 应用：多个生产者和多个消费者结束后，元素丢失或重复。
// 原理：四个生产者各发送 500 个互异值。消费者集合大小为 2000。队列有界。元素类型必须可平凡复制。索引原子不是始终无锁时，结果为不支持。
int main() {
  syslab::MpmcQueue<int, 1024> queue;
  if (!queue.is_lock_free()) {
    syslab_step("索引原子不是始终无锁，本节停止。");
    return syslab_unsupported("index-atomic-not-lock-free");
  }
  constexpr int kEach = 500;
  std::atomic<int> popped{0};
  std::mutex mutex;
  std::set<int> values;
  std::vector<std::thread> threads;
  for (int producer = 0; producer < 4; ++producer) {
    threads.emplace_back([&, producer]() {
      for (int item = 0; item < kEach; ++item) while (!queue.push(producer * kEach + item)) {}
    });
  }
  for (int consumer = 0; consumer < 4; ++consumer) {
    threads.emplace_back([&]() {
      while (popped.load() < 4 * kEach) {
        int item = 0;
        if (queue.pop(item)) {
          std::lock_guard<std::mutex> guard(mutex);
          values.insert(item);
          popped.fetch_add(1);
        }
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (values.size() != static_cast<std::size_t>(4 * kEach)) {
    syslab_step("互异值的数量与发送或压入的数量不一致。");
    return syslab_fail("values");
  }
  syslab_note("OBSERVED", "a failed push or pop means another thread claimed the position");
    syslab_step("消费者收到的互异值数量为 2000，与四个生产者发送的数量相同。");
  return syslab_pass();
}
