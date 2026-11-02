#include "experiments/common/check.hpp"
#include "syslab/synchronization/rw_lock.hpp"

#include <atomic>
#include <chrono>
#include <thread>

// 应用：后到的读者插到已经排队的写者前面，或两个写者的计数互相覆盖。
// 原理：已有共享持有者时，先排队的写者在共享持有者离开后进入；后到的读者等这个写者离开。两个写者各递增 100 次，计数为 200，写锁不重叠。

namespace {

bool wait_until(const std::chrono::steady_clock::time_point& deadline, syslab::RwLock& lock, bool writers) {
  while ((writers ? lock.waiting_writers() : lock.waiting_readers()) < 1) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    std::this_thread::yield();
  }
  return true;
}

}  // namespace

int main() {
  syslab::RwLock lock;
  lock.lock_shared();
  std::atomic<int> writer_ran{0};
  std::atomic<int> reader_before_writer{0};
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  std::thread writer([&]() {
    lock.lock();
    writer_ran.store(1, std::memory_order_release);
    lock.unlock();
  });
  if (!wait_until(deadline, lock, true)) {
    lock.unlock_shared();
    writer.join();
    syslab_step("写者没有排进等待队列。");
    return syslab_fail("writer-not-queued");
  }
  std::thread reader([&]() {
    lock.lock_shared();
    if (writer_ran.load(std::memory_order_acquire) != 1) {
      reader_before_writer.store(1, std::memory_order_relaxed);
    }
    lock.unlock_shared();
  });
  if (!wait_until(deadline, lock, false)) {
    lock.unlock_shared();
    writer.join();
    reader.join();
    syslab_step("后到的读者没有排进等待队列。");
    return syslab_fail("reader-not-queued");
  }
  lock.unlock_shared();
  writer.join();
  reader.join();
  if (writer_ran.load() != 1 || reader_before_writer.load() != 0) {
    syslab_step("后到的读者在已排队的写者之前进入。");
    return syslab_fail("write-preference");
  }

  int depth = 0;
  int writes = 0;
  int overlap = 0;
  auto write = [&]() {
    for (int step = 0; step < 100; ++step) {
      lock.lock();
      if (++depth != 1) {
        overlap = 1;
      }
      ++writes;
      --depth;
      lock.unlock();
    }
  };
  std::thread left(write);
  std::thread right(write);
  left.join();
  right.join();
  if (writes != 200 || overlap != 0) {
    syslab_step("两个写者的计数不是 200，或写锁发生重叠。");
    return syslab_fail("write-exclusion");
  }
  syslab_metric("writes", writes);
  syslab_note("OBSERVED", "a queued writer entered before the later reader; two writers did not overlap");
  syslab_step("后到的读者在已排队的写者之后进入。两个写者各递增 100 次，计数为 200，写锁没有重叠。");
  return syslab_pass();
}
