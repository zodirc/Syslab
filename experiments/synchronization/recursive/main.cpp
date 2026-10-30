#include "experiments/common/check.hpp"
#include "syslab/synchronization/recursive_mutex.hpp"
#include <cstdint>
#include <thread>
#include <vector>
// 应用：同一线程再次加锁后死锁，或者其他线程在持有者重入期间进入了临界区。
// 原理：拥有者线程可以连续 lock 两次，再 unlock 两次。其他线程之间仍然互斥。四线程计数为 2000。非拥有者不能借用递归计数进入临界区。
int main() {
  syslab::RecursiveMutex lock;
  lock.lock();
  lock.lock();
  lock.unlock();
  lock.unlock();
  std::uint64_t counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 500; ++step) {
        syslab::LockGuard<syslab::RecursiveMutex> guard(lock);
        ++counter;
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (counter != 2000) {
    syslab_step("临界区内的计数与预期不一致。");
    return syslab_fail("exclusion");
  }
  syslab_note("OBSERVED", "reentry is limited to the owning thread");
    syslab_step("同一线程连续加锁两次并解锁两次。随后四线程在递归互斥量内的计数为 2000。");
  return syslab_pass();
}
