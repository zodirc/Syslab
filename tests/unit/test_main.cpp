#include "syslab/atomic/epoch.hpp"
#include "syslab/atomic/lockfree_stack.hpp"
#include "syslab/atomic/mpmc_queue.hpp"
#include "syslab/atomic/spsc_ring.hpp"
#include "syslab/concurrency/scheduler.hpp"
#include "syslab/concurrency/work_stealing.hpp"
#include "syslab/core/error.hpp"
#include "syslab/core/sha256.hpp"
#include "syslab/core/yaml.hpp"
#include "syslab/experiment/lifecycle.hpp"
#include "syslab/memory/arena_allocator.hpp"
#include "syslab/memory/buddy_allocator.hpp"
#include "syslab/memory/freelist_allocator.hpp"
#include "syslab/memory/pool_allocator.hpp"
#include "syslab/memory/size_class_allocator.hpp"
#include "syslab/memory/thread_cache_allocator.hpp"
#include "syslab/runner/process.hpp"
#include "syslab/runtime/coroutine.hpp"
#include "syslab/runtime/smart_ptr.hpp"
#include "syslab/synchronization/barrier.hpp"
#include "syslab/synchronization/condition_variable.hpp"
#include "syslab/synchronization/mcs_lock.hpp"
#include "syslab/synchronization/mutex.hpp"
#include "syslab/synchronization/once.hpp"
#include "syslab/synchronization/recursive_mutex.hpp"
#include "syslab/synchronization/rw_lock.hpp"
#include "syslab/synchronization/semaphore.hpp"
#include "syslab/synchronization/spinlock.hpp"
#include "syslab/synchronization/ticket_lock.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_failed = 0;

void expect(bool condition, const char* expression) {
  if (!condition) {
    std::cerr << "FAIL " << expression << "\n";
    ++g_failed;
  }
}

#define EXPECT(condition) expect(static_cast<bool>(condition), #condition)

void test_sha_and_yaml() {
  EXPECT(syslab::to_hex(syslab::sha256(std::string("abc"))) ==
         "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  const syslab::Yaml root = syslab::Yaml::parse(
      "schema_version: 1\nexperiment:\n  id: AT-003\n  tags:\n    - atomic\n", "memory");
  EXPECT(root.require("schema_version").as_integer() == 1);
  EXPECT(root.require("experiment").require("id").as_scalar() == "AT-003");
  EXPECT(root.require("experiment").require("tags").as_sequence().size() == 1);
}

void test_lifecycle() {
  syslab::Lifecycle lifecycle("CPU-001");
  lifecycle.transition(syslab::ExperimentState::Validating, "check");
  bool threw = false;
  try {
    lifecycle.transition(syslab::ExperimentState::Passed, "skip");
  } catch (const syslab::Error& error) {
    threw = error.code() == syslab::Error::Code::State;
  }
  EXPECT(threw);
}

void test_arena_and_pool() {
  syslab::ArenaAllocator arena(64 * 1024);
  void* first = arena.allocate({32, 16, 1});
  void* second = arena.allocate({64, 32, 2});
  EXPECT(first != nullptr && second != nullptr);
  EXPECT(reinterpret_cast<std::uintptr_t>(second) % 32 == 0);
  std::memset(first, 0x11, 32);
  arena.deallocate(second, 64, 32);
  bool double_free = false;
  try {
    arena.deallocate(second, 64, 32);
  } catch (const syslab::Error&) {
    double_free = true;
  }
  EXPECT(double_free);
  syslab::PoolAllocator pool(32, 4, 16);
  void* blocks[4];
  for (int index = 0; index < 4; ++index) {
    blocks[index] = pool.allocate({32, 16, 0});
    EXPECT(blocks[index] != nullptr);
  }
  EXPECT(pool.allocate({32, 16, 0}) == nullptr);
  pool.deallocate(blocks[0], 32, 16);
  EXPECT(pool.allocate({32, 16, 0}) != nullptr);
}

void test_freelist_and_buddy() {
  syslab::FreeListAllocator allocator(64 * 1024);
  void* a = allocator.allocate({1000, 16, 0});
  void* b = allocator.allocate({1000, 16, 0});
  void* c = allocator.allocate({1000, 16, 0});
  EXPECT(a && b && c);
  allocator.deallocate(a, 1000, 16);
  allocator.deallocate(b, 1000, 16);
  allocator.deallocate(c, 1000, 16);
  void* large = allocator.allocate({2500, 16, 0});
  EXPECT(large != nullptr);
  allocator.deallocate(large, 2500, 16);

  syslab::BuddyAllocator buddy(64 * 1024);
  std::vector<void*> pointers;
  for (int index = 0; index < 8; ++index) {
    void* pointer = buddy.allocate({24, 8, 0});
    EXPECT(pointer != nullptr);
    std::memset(pointer, 0x5a, 24);
    pointers.push_back(pointer);
  }
  for (void* pointer : pointers) {
    buddy.deallocate(pointer, 24, 8);
  }
  EXPECT(buddy.allocate({128, 16, 0}) != nullptr);
}

void test_size_and_cache() {
  syslab::SizeClassAllocator classes(32);
  void* pointer = classes.allocate({24, 8, 0});
  EXPECT(pointer != nullptr);
  classes.deallocate(pointer, 24, 8);
  void* large = classes.allocate({8192, 16, 0});
  EXPECT(large != nullptr);
  classes.deallocate(large, 8192, 16);

  syslab::ThreadCacheAllocator cache(std::make_unique<syslab::SizeClassAllocator>(128));
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&cache]() {
      std::vector<void*> local;
      for (int item = 0; item < 40; ++item) {
        local.push_back(cache.allocate({32, 8, 0}));
      }
      for (void* block : local) {
        cache.deallocate(block, 32, 8);
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  const syslab::AllocationStats stats = cache.statistics();
  EXPECT(stats.has(syslab::StatField::Allocated));
  EXPECT(stats.allocated_bytes == 0);
}

template <typename Lock>
void hammer_lock(Lock& lock) {
  constexpr int kThreads = 4;
  constexpr int kIters = 2000;
  std::uint64_t counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < kThreads; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < kIters; ++step) {
        syslab::LockGuard<Lock> guard(lock);
        ++counter;
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  EXPECT(counter == static_cast<std::uint64_t>(kThreads * kIters));
}

void test_locks() {
  syslab::SpinLock spin;
  hammer_lock(spin);
  syslab::TicketLock ticket;
  hammer_lock(ticket);
  syslab::Mutex mutex;
  hammer_lock(mutex);
  syslab::McsLock mcs;
  std::uint64_t counter = 0;
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 1000; ++step) {
        syslab::McsGuard guard(mcs);
        ++counter;
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  EXPECT(counter == 4000);

  syslab::RwLock rw;
  std::atomic<int> readers{0};
  int writes = 0;
  threads.clear();
  for (int index = 0; index < 3; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 200; ++step) {
        rw.lock_shared();
        readers.fetch_add(1);
        readers.fetch_sub(1);
        rw.unlock_shared();
      }
    });
  }
  threads.emplace_back([&]() {
    for (int step = 0; step < 100; ++step) {
      rw.lock();
      ++writes;
      rw.unlock();
    }
  });
  for (std::thread& thread : threads) {
    thread.join();
  }
  EXPECT(writes == 100);

  syslab::RecursiveMutex recursive;
  recursive.lock();
  recursive.lock();
  recursive.unlock();
  recursive.unlock();

  syslab::Semaphore semaphore(2);
  std::atomic<int> active{0};
  std::atomic<int> max_active{0};
  threads.clear();
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      for (int step = 0; step < 50; ++step) {
        semaphore.acquire();
        int now = active.fetch_add(1) + 1;
        int seen = max_active.load();
        while (now > seen && !max_active.compare_exchange_weak(seen, now)) {
        }
        active.fetch_sub(1);
        semaphore.release();
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  EXPECT(max_active.load() <= 2);

  syslab::Barrier barrier(4);
  std::atomic<int> phase{0};
  threads.clear();
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&]() {
      phase.fetch_add(1);
      barrier.arrive_and_wait();
      EXPECT(phase.load() == 4);
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }

  syslab::OnceFlag once;
  std::atomic<int> calls{0};
  threads.clear();
  for (int index = 0; index < 8; ++index) {
    threads.emplace_back([&]() { once.call_once([&]() { calls.fetch_add(1); }); });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  EXPECT(calls.load() == 1);

  syslab::Mutex gate;
  syslab::ConditionVariable condition;
  bool ready = false;
  std::thread waiter([&]() {
    syslab::LockGuard<syslab::Mutex> guard(gate);
    condition.wait(gate, [&]() { return ready; });
  });
  {
    syslab::LockGuard<syslab::Mutex> guard(gate);
    ready = true;
    condition.notify_one();
  }
  waiter.join();
}

void test_queues_and_stack() {
  syslab::SpscRing<int, 1024> ring;
  std::thread producer([&]() {
    for (int value = 0; value < 10000; ++value) {
      while (!ring.push(value)) {
      }
    }
  });
  int expected = 0;
  int value = 0;
  while (expected < 10000) {
    if (ring.pop(value)) {
      EXPECT(value == expected);
      ++expected;
    }
  }
  producer.join();

  syslab::MpmcQueue<int, 1024> queue;
  constexpr int kProducers = 4;
  constexpr int kEach = 1000;
  std::atomic<int> popped{0};
  std::vector<std::thread> threads;
  for (int producer_index = 0; producer_index < kProducers; ++producer_index) {
    threads.emplace_back([&, producer_index]() {
      for (int item = 0; item < kEach; ++item) {
        while (!queue.push(producer_index * kEach + item)) {
        }
      }
    });
  }
  std::set<int> values;
  std::mutex values_mutex;
  for (int consumer = 0; consumer < kProducers; ++consumer) {
    threads.emplace_back([&]() {
      while (popped.load() < kProducers * kEach) {
        int item = 0;
        if (queue.pop(item)) {
          std::lock_guard<std::mutex> guard(values_mutex);
          values.insert(item);
          popped.fetch_add(1);
        }
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  EXPECT(values.size() == static_cast<std::size_t>(kProducers * kEach));

  syslab::LockFreeStack<int> stack;
  threads.clear();
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&, index]() {
      for (int item = 0; item < 200; ++item) {
        stack.push(index * 200 + item);
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  std::set<int> popped_values;
  int item = 0;
  while (stack.pop(item)) {
    popped_values.insert(item);
  }
  EXPECT(popped_values.size() == 800);
  EXPECT(stack.is_lock_free());
}

std::atomic<int> g_epoch_deleted{0};

void epoch_reclaim(void* pointer) {
  g_epoch_deleted.fetch_add(1, std::memory_order_relaxed);
  delete static_cast<int*>(pointer);
}

void test_epoch() {
  g_epoch_deleted.store(0);
  syslab::EpochDomain epoch;
  epoch.pin();
  epoch.retire(new int(1), epoch_reclaim);
  EXPECT(g_epoch_deleted.load() == 0);
  epoch.unpin();
  epoch.retire(new int(2), epoch_reclaim);
  epoch.retire(new int(3), epoch_reclaim);
  EXPECT(g_epoch_deleted.load() == 2);
}

void test_fiber_coroutine_and_steal() {
  syslab::CooperativeScheduler scheduler;
  std::string log;
  scheduler.spawn([&]() {
    log += "A1";
    syslab::CooperativeScheduler::yield();
    log += "A2";
  });
  scheduler.spawn([&]() {
    log += "B1";
    syslab::CooperativeScheduler::yield();
    log += "B2";
  });
  scheduler.run();
  EXPECT(log == "A1B1A2B2");

  syslab::CoroutineScheduler coroutines;
  std::string coro_log;
  auto task = [&]() -> syslab::Task {
    coro_log += "A";
    co_await syslab::yield_to(coroutines);
    coro_log += "B";
  };
  coroutines.spawn(task());
  coroutines.spawn([&]() -> syslab::Task {
    coro_log += "C";
    co_await syslab::yield_to(coroutines);
    coro_log += "D";
    co_return;
  }());
  coroutines.run();
  EXPECT(coro_log == "ACBD");

  std::atomic<std::uint64_t> sum{0};
  syslab::parallel_for(4, 1000, [&](std::size_t index) { sum.fetch_add(index, std::memory_order_relaxed); });
  EXPECT(sum.load() == 999ull * 1000ull / 2ull);

  int destroyed = 0;
  struct Node {
    int* counter = nullptr;
    ~Node() {
      if (counter != nullptr) {
        ++*counter;
      }
    }
  };
  syslab::WeakPtr<Node> weak;
  {
    syslab::SharedPtr<Node> shared = syslab::make_shared<Node>();
    shared->counter = &destroyed;
    weak = syslab::WeakPtr<Node>(shared);
    EXPECT(shared.use_count() == 1);
    auto locked = weak.lock();
    EXPECT(locked.use_count() == 2);
  }
  EXPECT(destroyed == 1);
  EXPECT(weak.expired());
}

void test_runner() {
  syslab::CommandRequest request;
  request.argv = {"/bin/true"};
  request.inherit_environment = true;
  request.timeout_ms = 2000;
  const syslab::CommandResult ok = syslab::run_command(request);
  EXPECT(ok.exit_code == 0);
  EXPECT(ok.termination == "completed");
  request.argv = {"/bin/sleep", "5"};
  request.timeout_ms = 200;
  const syslab::CommandResult timed = syslab::run_command(request);
  EXPECT(timed.timed_out);
  EXPECT(timed.termination == "timed_out");
}

}  // namespace

int main() {
  try {
    test_sha_and_yaml();
    test_lifecycle();
    test_arena_and_pool();
    test_freelist_and_buddy();
    test_size_and_cache();
    test_locks();
    test_queues_and_stack();
    test_epoch();
    test_fiber_coroutine_and_steal();
    test_runner();
  } catch (const std::exception& error) {
    std::cerr << "uncaught " << error.what() << "\n";
    return 1;
  }
  if (g_failed != 0) {
    std::cerr << g_failed << " assertion(s) failed\n";
    return 1;
  }
  std::cout << "unit tests passed\n";
  return 0;
}
