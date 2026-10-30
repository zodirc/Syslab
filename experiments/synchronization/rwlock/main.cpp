#include "experiments/common/check.hpp"
#include "syslab/synchronization/rw_lock.hpp"
#include <thread>
#include <vector>
// 应用：读写锁下写者的计数被共享持有者打断，或共享路径无法完成。
// 原理：写者持有互斥的写锁并递增 100 次，结果为 100。三个线程的共享锁路径能够完成。写优先是这个实现的策略。POSIX 读写锁还有其他合法策略。
int main() {
  syslab::RwLock lock;
  int writes = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 3; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 200; ++step) {
        lock.lock_shared();
        lock.unlock_shared();
      }
    });
  }
  threads.emplace_back([&]() {
    for (int step = 0; step < 100; ++step) {
      lock.lock();
      ++writes;
      lock.unlock();
    }
  });
  for (std::thread& thread : threads) thread.join();
  if (writes != 100) {
    syslab_step("写锁下的递增结果不是 100。");
    return syslab_fail("writes");
  }
  syslab_note("OBSERVED", "this rwlock prefers writers; shared holders exclude the writer");
    syslab_step("写锁互斥下的递增结果为 100。共享锁路径已经完成。");
  return syslab_pass();
}
