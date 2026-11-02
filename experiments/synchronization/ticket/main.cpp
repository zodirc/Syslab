#include "experiments/common/check.hpp"
#include "syslab/synchronization/mutex.hpp"
#include "syslab/synchronization/ticket_lock.hpp"

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

// 应用：票据锁保护的计数偏小，或后取号的线程先进入临界区。
// 原理：主线程先持有锁。第一个线程取到下一号并等待，第二个线程再取号。主线程解锁后，先取号的线程先进入。四个线程另各递增 2000 次，计数为 8000。

namespace {

bool wait_until(const std::chrono::steady_clock::time_point& deadline, const syslab::TicketLock& lock, std::uint32_t tickets) {
  while (lock.issued() < tickets) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    std::this_thread::yield();
  }
  return true;
}

}  // namespace

int main() {
  syslab::TicketLock lock;
  lock.lock();
  std::vector<int> order;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  std::thread first([&]() {
    lock.lock();
    order.push_back(1);
    lock.unlock();
  });
  if (!wait_until(deadline, lock, 2)) {
    lock.unlock();
    first.join();
    syslab_step("第一个线程没有取到号。");
    return syslab_fail("ticket-not-taken");
  }
  std::thread second([&]() {
    lock.lock();
    order.push_back(2);
    lock.unlock();
  });
  if (!wait_until(deadline, lock, 3)) {
    lock.unlock();
    first.join();
    second.join();
    syslab_step("第二个线程没有取到号。");
    return syslab_fail("ticket-not-taken");
  }
  lock.unlock();
  first.join();
  second.join();
  if (order.size() != 2 || order[0] != 1 || order[1] != 2) {
    syslab_step("进入顺序不是取号顺序。");
    return syslab_fail("ticket-order");
  }

  std::uint64_t counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 2000; ++step) {
        syslab::LockGuard<syslab::TicketLock> guard(lock);
        ++counter;
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  if (counter != 8000) {
    syslab_step("临界区内的计数与预期不一致。");
    return syslab_fail("exclusion");
  }
  syslab_metric("counter", static_cast<long long>(counter));
  syslab_note("OBSERVED", "the earlier ticket entered first; four threads then incremented to 8000");
  syslab_step("先取号的线程先进入。四个线程各递增 2000 次，计数为 8000。");
  return syslab_pass();
}
