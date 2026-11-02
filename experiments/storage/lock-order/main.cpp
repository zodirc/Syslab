#include "experiments/common/check.hpp"

#include <pthread.h>

#include <atomic>
#include <cerrno>
#include <thread>

// 原理：两边都持有第一把锁时，对另一把锁的 trylock 返回 EBUSY，更新数是 0。相同顺序完成后更新数是 2。
namespace {

pthread_mutex_t locks[2] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};
std::atomic<int> ready{0};
std::atomic<int> applied{0};

void opposite(int mine, int other, int* busy) {
  pthread_mutex_lock(&locks[mine]);
  ready.fetch_add(1);
  while (ready.load() < 2) {
    std::this_thread::yield();
  }
  const int result = pthread_mutex_trylock(&locks[other]);
  if (result == 0) {
    pthread_mutex_unlock(&locks[other]);
    applied.fetch_add(1);
  }
  *busy = result;
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
  int first_busy = 0;
  int second_busy = 0;
  std::thread left(opposite, 0, 1, &first_busy);
  std::thread right(opposite, 1, 0, &second_busy);
  left.join();
  right.join();
  syslab_metric("trylock_0", first_busy);
  syslab_metric("trylock_1", second_busy);
  if (first_busy != EBUSY || second_busy != EBUSY || applied.load() != 0) {
    syslab_step("两边都持有第一把锁时，对另一把锁的 trylock 没有返回 EBUSY。");
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
  syslab_note("OS", "while each thread holds its first mutex, trylock of the other returns EBUSY");
  syslab_step("两边都持有第一把锁时，对另一把锁的 trylock 返回 EBUSY，更新数是 0。相同顺序完成后更新数是 2。");
  return syslab_pass();
}
