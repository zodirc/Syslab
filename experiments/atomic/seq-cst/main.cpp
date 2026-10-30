#include "experiments/common/check.hpp"
#include "syslab/synchronization/barrier.hpp"
#include <atomic>
#include <thread>
// 应用：两个读者对两个写入的观察顺序互相交叉，业务上认为全局顺序被破坏。
// 原理：seq_cst 禁止 r1 为 1、r2 为 0、r3 为 1、r4 为 0 这一组结果。200 次没有出现，只说明这批样本没有违反约束。
int main() {
  for (int trial = 0; trial < 200; ++trial) {
    std::atomic<int> x{0};
    std::atomic<int> y{0};
    syslab::Barrier start(4);
    int r1 = 0, r2 = 0, r3 = 0, r4 = 0;
    std::thread t1([&]() { start.arrive_and_wait(); x.store(1, std::memory_order_seq_cst); });
    std::thread t2([&]() { start.arrive_and_wait(); y.store(1, std::memory_order_seq_cst); });
    std::thread t3([&]() {
      start.arrive_and_wait();
      r1 = x.load(std::memory_order_seq_cst);
      r2 = y.load(std::memory_order_seq_cst);
    });
    std::thread t4([&]() {
      start.arrive_and_wait();
      r3 = y.load(std::memory_order_seq_cst);
      r4 = x.load(std::memory_order_seq_cst);
    });
    t1.join(); t2.join(); t3.join(); t4.join();
    if (r1 == 1 && r2 == 0 && r3 == 1 && r4 == 0) {
    syslab_step("样本中出现了 seq_cst 禁止的交叉结果。");
    return syslab_fail("iriw");
  }
  }
  syslab_note("LANG", "that IRIW outcome is forbidden for seq_cst");
  syslab_note("OBSERVED", "absence in 200 trials is a sample, not a proof that every execution is ordered");
    syslab_step("200 次样本中没有出现被 seq_cst 禁止的那组交叉结果。");
  return syslab_pass();
}
