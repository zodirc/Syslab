#include "experiments/common/check.hpp"
#include <atomic>
#include <chrono>
#include <thread>
// 应用：两个线程分别递增相邻的计数器，耗时明显长于分开存放的计数器，计数本身是否完成尚未确认。
// 原理：相邻布局和按缓存行填充的布局都完成递增，两次之和都是 400000。耗时差取决于微架构，不作为通过条件。
int main() {
  struct Adjacent { std::atomic<int> left{0}; std::atomic<int> right{0}; };
  struct Padded { alignas(64) std::atomic<int> left{0}; alignas(64) std::atomic<int> right{0}; };
  auto run = [](auto& pair) {
    const auto start = std::chrono::steady_clock::now();
    std::thread a([&]() { for (int i = 0; i < 200000; ++i) pair.left.fetch_add(1, std::memory_order_relaxed); });
    std::thread b([&]() { for (int i = 0; i < 200000; ++i) pair.right.fetch_add(1, std::memory_order_relaxed); });
    a.join(); b.join();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    return std::pair<int, long long>{pair.left.load() + pair.right.load(), elapsed};
  };
  Adjacent adjacent;
  Padded padded;
  const auto left = run(adjacent);
  const auto right = run(padded);
  syslab_metric("adjacent_us", left.second);
  syslab_metric("padded_us", right.second);
  if (left.first != 400000 || right.first != 400000) {
    syslab_step("递增之和与预期不一致。");
    return syslab_fail("count");
  }
  syslab_note("MICROARCH", "a slower adjacent pair is common and not required for a pass");
    syslab_step("相邻布局和填充布局的递增之和都是 400000。耗时只作为样本记录。");
  return syslab_pass();
}
