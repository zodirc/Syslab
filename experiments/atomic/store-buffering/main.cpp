#include "experiments/common/check.hpp"
#include "syslab/synchronization/barrier.hpp"
#include <atomic>
#include <thread>
// 应用：两个线程各自先写自己的变量再读对方的变量，两边都读到 0。
// 原理：两个线程的存储和加载使用 memory_order_seq_cst。300 次试验中两个加载没有同时为 0。
int main() {
  for (int trial = 0; trial < 300; ++trial) {
    std::atomic<int> x{0};
    std::atomic<int> y{0};
    syslab::Barrier start(2);
    int r1 = 1;
    int r2 = 1;
    std::thread first([&]() {
      start.arrive_and_wait();
      x.store(1, std::memory_order_seq_cst);
      r1 = y.load(std::memory_order_seq_cst);
    });
    std::thread second([&]() {
      start.arrive_and_wait();
      y.store(1, std::memory_order_seq_cst);
      r2 = x.load(std::memory_order_seq_cst);
    });
    first.join();
    second.join();
    if (r1 == 0 && r2 == 0) {
    syslab_step("seq_cst 试验中两个加载同时为 0。");
    return syslab_fail("both-zero");
  }
  }
  syslab_note("LANG", "300 seq_cst trials did not observe both loads as 0");
    syslab_step("300 次 seq_cst 试验中，没有出现两个加载同时为 0。");
  return syslab_pass();
}
