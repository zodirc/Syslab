#include "experiments/common/check.hpp"

#include <pthread.h>
#include <time.h>

#include <atomic>
#include <cerrno>
#include <thread>

// 应用：两行更新互相等时，先核对两把锁的获取顺序。
// 原理：相同顺序可以都进入临界区。相反顺序在超时后放弃，不写入更新。
namespace {

pthread_mutex_t locks[2] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};
std::atomic<int> ready{0};
std::atomic<int> applied{0};

void opposite(int mine, int other, int* timed_out) {
  pthread_mutex_lock(&locks[mine]);
  ready.fetch_add(1);
  while (ready.load() < 2) {
    std::this_thread::yield();
  }
  timespec deadline{};
  clock_gettime(CLOCK_REALTIME, &deadline);
  deadline.tv_nsec += 300000000L;
  if (deadline.tv_nsec >= 1000000000L) {
    deadline.tv_sec += 1;
    deadline.tv_nsec -= 1000000000L;
  }
  const int result = pthread_mutex_timedlock(&locks[other], &deadline);
  if (result == 0) {
    pthread_mutex_unlock(&locks[other]);
    applied.fetch_add(1);
    *timed_out = 0;
  } else {
    *timed_out = result;
  }
  pthread_mutex_unlock(&locks[mine]);
}

void same_order() {
  pthread_mutex_lock(&locks[0]);
  pthread_mutex_lock(&locks[1]);
  applied.fetch_add(1);
  pthread_mutex_unlock(&locks[1]);
  pthread_mutex_unlock(&locks[0]);
}

}  // namespace

int main() {
  int first_timeout = 0;
  int second_timeout = 0;
  std::thread left(opposite, 0, 1, &first_timeout);
  std::thread right(opposite, 1, 0, &second_timeout);
  left.join();
  right.join();
  if (first_timeout != ETIMEDOUT || second_timeout != ETIMEDOUT || applied.load() != 0) {
    syslab_step("相反的加锁顺序没有在超时后放弃更新。");
    return syslab_fail("order");
  }
  std::thread first(same_order);
  std::thread second(same_order);
  first.join();
  second.join();
  if (applied.load() != 2) {
    syslab_step("相同的加锁顺序没有完成两次更新。");
    return syslab_fail("update");
  }
  syslab_metric("ordered_updates", applied.load());
  syslab_note("OS", "opposite mutex order reaches ETIMEDOUT and does not publish the update");
  syslab_step("相反顺序两次都是 ETIMEDOUT，更新数仍是 0。相同顺序完成后更新数是 2。");
  return syslab_pass();
}
