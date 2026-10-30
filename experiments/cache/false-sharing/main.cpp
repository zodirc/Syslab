#include "experiments/common/check.hpp"
#include <atomic>
#include <chrono>
#include <thread>
// 应用：两个计数器放在相邻地址时变慢，需要先确认两种布局都完成了递增。
// 原理：相邻数组元素和分开对齐的两个原子，计数和都是 200000。相邻元素是否落在同一缓存行取决于 ABI 布局。耗时差异不是通过条件。
int main() {
  alignas(64) std::atomic<int> adjacent[2] {};
  alignas(64) std::atomic<int> padded_left{0};
  alignas(64) std::atomic<int> padded_right{0};
  auto elapsed = [](auto&& body) {
    const auto start = std::chrono::steady_clock::now();
    body();
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
  };
  const auto adjacent_us = elapsed([&]() {
    std::thread a([&]() { for (int i = 0; i < 100000; ++i) adjacent[0].fetch_add(1, std::memory_order_relaxed); });
    std::thread b([&]() { for (int i = 0; i < 100000; ++i) adjacent[1].fetch_add(1, std::memory_order_relaxed); });
    a.join(); b.join();
  });
  const auto padded_us = elapsed([&]() {
    std::thread a([&]() { for (int i = 0; i < 100000; ++i) padded_left.fetch_add(1, std::memory_order_relaxed); });
    std::thread b([&]() { for (int i = 0; i < 100000; ++i) padded_right.fetch_add(1, std::memory_order_relaxed); });
    a.join(); b.join();
  });
  syslab_metric("adjacent_us", adjacent_us);
  syslab_metric("padded_us", padded_us);
  if (adjacent[0].load() + adjacent[1].load() != 200000) {
    syslab_step("相邻布局的计数和不是 200000。");
    return syslab_fail("adjacent");
  }
  if (padded_left.load() + padded_right.load() != 200000) {
    syslab_step("分开布局的计数和不是 200000。");
    return syslab_fail("padded");
  }
  syslab_note("MICROARCH", "false sharing is a cache-line effect; the pass condition is the totals");
    syslab_step("相邻布局和分开布局的计数和都是 200000。");
  return syslab_pass();
}
