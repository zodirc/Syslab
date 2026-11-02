#include "experiments/common/check.hpp"
#include "syslab/synchronization/once.hpp"
#include <atomic>
#include <thread>
#include <vector>
// 原理：八个线程调用同一个 OnceFlag 的 call_once。比较交换把状态从 0 改成 1 的线程运行函数。完成后状态写成 2。初始化函数进入一次。本节的函数不抛出异常。
int main() {
  syslab::OnceFlag once;
  std::atomic<int> calls{0};
  std::vector<std::thread> threads;
  for (int index = 0; index < 8; ++index) {
    threads.emplace_back([&]() { once.call_once([&]() { calls.fetch_add(1); }); });
  }
  for (std::thread& thread : threads) thread.join();
  if (calls.load() != 1) {
    syslab_step("初始化函数进入了不止一次。");
    return syslab_fail("once");
  }
  syslab_metric("calls", calls.load());
  syslab_note("OBSERVED", "the compare-exchange winner ran the function once and stored state 2");
  syslab_step("八个线程的初始化函数只进入一次。");
  return syslab_pass();
}
