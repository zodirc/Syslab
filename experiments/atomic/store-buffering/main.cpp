#include "experiments/common/check.hpp"
#include "syslab/synchronization/barrier.hpp"
#include <atomic>
#include <thread>
// 应用：两个线程各自先写自己的变量再读对方的变量，两边都读到 0。
// 原理：seq_cst 不允许这次存储缓冲结果中两个加载同时为 0。300 次试验中不出现该结果。relaxed 或 acq_rel 允许该结果，本节不把那种允许的结果当成失败。
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
  syslab_note("LANG", "seq_cst forbids the both-zero store-buffering outcome");
  syslab_note("LANG", "the same shape with relaxed or acq_rel is allowed to observe both zero and is not failed here");
    syslab_step("300 次 seq_cst 试验中，没有出现两个加载同时为 0。");
  return syslab_pass();
}
