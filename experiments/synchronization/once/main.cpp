#include "experiments/common/check.hpp"
#include "syslab/synchronization/mutex.hpp"
#include "syslab/synchronization/once.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
// 应用：初始化函数执行了多次，或者把慢路径计数当成线程已经在 futex 上睡眠。
// 原理：锁已被持有时，另一个线程进入 Mutex 的慢路径，slow_path_count 在解锁前大于 0。该计数在快速比较交换失败时增加，不证明 futex 已经睡眠。八个线程调用 OnceFlag，初始化函数只进入一次。
int main() {
  syslab::Mutex mutex;
  mutex.lock();
  std::atomic<int> started{0};
  std::thread contender([&]() {
    started.store(1);
    mutex.lock();
    mutex.unlock();
  });
  while (started.load() == 0) std::this_thread::yield();
  for (int spin = 0; spin < 100 && mutex.slow_path_count() == 0; ++spin) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  const auto slow = mutex.slow_path_count();
  mutex.unlock();
  contender.join();
  if (slow == 0) {
    syslab_step("锁已被持有时，slow_path_count 没有增加。");
    return syslab_fail("slow-path");
  }
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
  syslab_metric("slow_path", static_cast<long long>(slow));
  syslab_note("OBSERVED", "slow_path_count increments when the fast compare-exchange misses; it does not prove the futex slept");
    syslab_step("锁已被持有时，slow_path_count 在解锁前大于 0。八个线程的初始化函数只进入一次。");
  return syslab_pass();
}
