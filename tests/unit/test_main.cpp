#include "syslab/build/build_manager.hpp"
#include "syslab/atomic/epoch.hpp"
#include "syslab/atomic/lockfree_stack.hpp"
#include "syslab/atomic/mpmc_queue.hpp"
#include "syslab/atomic/spsc_ring.hpp"
#include "syslab/concurrency/scheduler.hpp"
#include "syslab/concurrency/work_stealing.hpp"
#include "syslab/core/error.hpp"
#include "syslab/core/fs.hpp"
#include "syslab/core/sha256.hpp"
#include "syslab/core/yaml.hpp"
#include "syslab/experiment/catalog.hpp"
#include "syslab/experiment/lifecycle.hpp"
#include "syslab/experiment/registry.hpp"
#include "syslab/experiment/manifest.hpp"
#include "syslab/experiment/session.hpp"
#include "syslab/observation/protocol.hpp"
#include "syslab/observation/report.hpp"
#include "syslab/platform/capability.hpp"
#include "syslab/platform/environment.hpp"
#include "learn/internal.hpp"
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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
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
  ticket.lock();
  std::vector<int> ticket_order;
  const auto ticket_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  std::thread ticket_first([&]() {
    ticket.lock();
    ticket_order.push_back(1);
    ticket.unlock();
  });
  while (ticket.issued() < 2 && std::chrono::steady_clock::now() < ticket_deadline) {
    std::this_thread::yield();
  }
  std::thread ticket_second([&]() {
    ticket.lock();
    ticket_order.push_back(2);
    ticket.unlock();
  });
  while (ticket.issued() < 3 && std::chrono::steady_clock::now() < ticket_deadline) {
    std::this_thread::yield();
  }
  ticket.unlock();
  ticket_first.join();
  ticket_second.join();
  EXPECT(ticket_order.size() == 2 && ticket_order[0] == 1 && ticket_order[1] == 2);
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
  syslab::McsNode node;
  mcs.lock(node);
  mcs.unlock(node);
  mcs.lock(node);
  mcs.unlock(node);

  syslab::RwLock rw;
  rw.lock_shared();
  std::atomic<int> writer_ran{0};
  std::atomic<int> reader_before_writer{0};
  const auto rw_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  std::thread waiting_writer([&]() {
    rw.lock();
    writer_ran.store(1, std::memory_order_release);
    rw.unlock();
  });
  while (rw.waiting_writers() < 1 && std::chrono::steady_clock::now() < rw_deadline) {
    std::this_thread::yield();
  }
  std::thread later_reader([&]() {
    rw.lock_shared();
    if (writer_ran.load(std::memory_order_acquire) != 1) {
      reader_before_writer.store(1, std::memory_order_relaxed);
    }
    rw.unlock_shared();
  });
  while (rw.waiting_readers() < 1 && std::chrono::steady_clock::now() < rw_deadline) {
    std::this_thread::yield();
  }
  rw.unlock_shared();
  waiting_writer.join();
  later_reader.join();
  EXPECT(writer_ran.load() == 1);
  EXPECT(reader_before_writer.load() == 0);
  int depth = 0;
  int writes = 0;
  int overlap = 0;
  auto write = [&]() {
    for (int step = 0; step < 100; ++step) {
      rw.lock();
      if (++depth != 1) {
        overlap = 1;
      }
      ++writes;
      --depth;
      rw.unlock();
    }
  };
  std::thread left(write);
  std::thread right(write);
  left.join();
  right.join();
  EXPECT(writes == 200);
  EXPECT(overlap == 0);

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
  auto second = [&]() -> syslab::Task {
    coro_log += "C";
    co_await syslab::yield_to(coroutines);
    coro_log += "D";
    co_return;
  };
  coroutines.spawn(task());
  coroutines.spawn(second());
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

void test_dependencies_and_digest() {
  const std::vector<std::string> dependencies =
      syslab::parse_make_dependencies("out: a.cpp b.hpp \\\n c\\ d.hpp\n");
  EXPECT(dependencies.size() == 3);
  EXPECT(dependencies.size() > 0 && dependencies[0] == "a.cpp");
  EXPECT(dependencies.size() > 1 && dependencies[1] == "b.hpp");
  EXPECT(dependencies.size() > 2 && dependencies[2] == "c d.hpp");
  const std::vector<std::string> split = syslab::parse_make_dependencies("out.o : only.hpp\n");
  EXPECT(split.size() == 1 && split[0] == "only.hpp");

  const std::string directory = "/tmp/syslab-digest-test";
  syslab::create_directories(directory);
  const std::string first = syslab::join_path(directory, "first.hpp");
  const std::string second = syslab::join_path(directory, "second.hpp");
  syslab::write_file(first, "alpha\n");
  syslab::write_file(second, "beta\n");
  std::string reason;
  const std::string forward = syslab::digest_files({second, first}, &reason);
  const std::string reverse = syslab::digest_files({first, second, first}, &reason);
  EXPECT(reason.empty());
  EXPECT(forward.size() == 64);
  EXPECT(forward == reverse);
  syslab::write_file(second, "beta!\n");
  const std::string changed = syslab::digest_files({first, second}, &reason);
  EXPECT(changed != forward);
  const std::string missing = syslab::digest_files({syslab::join_path(directory, "absent.hpp")}, &reason);
  EXPECT(missing == "unavailable");
  EXPECT(!reason.empty());
}

void test_catalog() {
#ifdef SYSLAB_SOURCE_DIR
  syslab::Registry registry;
  registry.load_tree(syslab::join_path(SYSLAB_SOURCE_DIR, "experiments"));
  const std::vector<std::string> issues = syslab::check_catalog(SYSLAB_SOURCE_DIR, registry);
  for (const std::string& issue : issues) {
    std::cerr << "catalog " << issue << "\n";
  }
  EXPECT(issues.empty());
  EXPECT(registry.ids().size() == 94);
#else
  EXPECT(false && "SYSLAB_SOURCE_DIR is not defined");
#endif
}

void test_protocol() {
  const syslab::ProtocolRecord ok = syslab::parse_protocol(
      "SYSLAB_STEP hello\nSYSLAB_METRIC cycles=3\nSYSLAB_NOTE basis=LANG text=rule\n"
      "SYSLAB_RESULT status=pass reason=ok\nnoise\n");
  EXPECT(ok.errors.empty());
  EXPECT(ok.status == "pass");
  EXPECT(ok.reason == "ok");
  EXPECT(ok.steps.size() == 1 && ok.steps[0] == "hello");
  EXPECT(ok.metrics.size() == 1 && ok.metrics[0].name == "cycles" && ok.metrics[0].value == "3");
  EXPECT(ok.notes.size() == 1 && ok.notes[0].basis == "LANG" && ok.notes[0].text == "rule");

  const syslab::ProtocolRecord repeated =
      syslab::parse_protocol("SYSLAB_RESULT status=pass\nSYSLAB_RESULT status=fail\n");
  EXPECT(!repeated.errors.empty());
  EXPECT(repeated.status == "pass");

  const syslab::ProtocolRecord basis = syslab::parse_protocol("SYSLAB_NOTE basis=NOPE text=x\n");
  EXPECT(!basis.errors.empty());
  EXPECT(basis.notes.empty());

  const syslab::ProtocolRecord metric = syslab::parse_protocol("SYSLAB_METRIC cycles\n");
  EXPECT(!metric.errors.empty());
  EXPECT(metric.metrics.empty());

  const syslab::ProtocolRecord empty_step = syslab::parse_protocol("SYSLAB_STEP \n");
  EXPECT(!empty_step.errors.empty());
  EXPECT(empty_step.steps.empty());
}

void test_clean_artifacts() {
  const std::string root = "/tmp/syslab-clean-test";
  syslab::remove_tree(root);
  syslab::create_directories(root);
  const std::string old_run = syslab::join_path(root, "runs/batch-20260101T000000-aaaa");
  const std::string new_run = syslab::join_path(root, "runs/batch-20260102T000000-bbbb");
  const std::string old_build = syslab::join_path(root, "builds/build-old");
  const std::string new_build = syslab::join_path(root, "builds/build-new");
  syslab::create_directories(old_run);
  syslab::create_directories(new_run);
  syslab::create_directories(old_build);
  syslab::create_directories(new_build);
  const std::string old_exe = syslab::join_path(old_build, "prog");
  const std::string new_exe = syslab::join_path(new_build, "prog");
  syslab::write_file(old_exe, "old");
  syslab::write_file(new_exe, "new");
  syslab::write_file(syslab::join_path(old_run, "run-old.json"),
                     "{\"experiment_id\":\"CPU-001\",\"build_id\":\"build-old\"}");
  syslab::write_file(syslab::join_path(new_run, "run-new.json"),
                     "{\"experiment_id\":\"CPU-001\",\"build_id\":\"build-new\"}");
  syslab::write_file(syslab::join_path(old_build, "build.json"),
                     std::string("{\"build_id\":\"build-old\",\"link_arguments\":[\"-o\",\"") + old_exe + "\"]}");
  syslab::write_file(syslab::join_path(new_build, "build.json"),
                     std::string("{\"build_id\":\"build-new\",\"link_arguments\":[\"-o\",\"") + new_exe + "\"]}");
  syslab::create_directories(syslab::join_path(root, "latest"));
  syslab::write_file(syslab::join_path(root, "latest/CPU-001.txt"), old_run + "\n");
  syslab::write_file(syslab::join_path(root, "index.jsonl"), "{\"experiment_id\":\"CPU-001\"}\n");

  const std::string kept_cache = syslab::join_path(root, "cache/prefix/kept");
  const std::string stale_cache = syslab::join_path(root, "cache/prefix/stale");
  syslab::create_directories(kept_cache);
  syslab::create_directories(stale_cache);
  syslab::write_file(syslab::join_path(kept_cache, "entry.json"),
                     std::string("{\"build_id\":\"build-z\",\"executable\":\"") + new_exe + "\"}");
  syslab::write_file(syslab::join_path(stale_cache, "entry.json"),
                     "{\"build_id\":\"build-a\",\"executable\":\"/tmp/syslab-clean-test/missing\"}");

  const syslab::CleanReport report = syslab::clean_artifacts(root, 1);
  EXPECT(report.runs_removed == 1);
  EXPECT(report.builds_removed == 1);
  EXPECT(!syslab::path_exists(old_run));
  EXPECT(syslab::path_exists(new_run));
  EXPECT(!syslab::path_exists(old_build));
  EXPECT(syslab::path_exists(new_build));
  EXPECT(syslab::path_exists(kept_cache));
  EXPECT(!syslab::path_exists(stale_cache));
  const std::string index = syslab::read_file(syslab::join_path(root, "index.jsonl"));
  EXPECT(index.find("batch-20260102T000000-bbbb") != std::string::npos);
  EXPECT(index.find("batch-20260101T000000-aaaa") == std::string::npos);
  const std::string latest = syslab::read_file(syslab::join_path(root, "latest/CPU-001.txt"));
  EXPECT(latest.find("batch-20260102T000000-bbbb") != std::string::npos);

  const std::string extra_cache = syslab::join_path(root, "cache/prefix/extra");
  syslab::create_directories(extra_cache);
  syslab::write_file(syslab::join_path(extra_cache, "entry.json"),
                     "{\"build_id\":\"build-m\",\"executable\":\"/tmp/syslab-clean-test/other\"}");
  syslab::write_file("/tmp/syslab-clean-test/other", "x");
  const int removed = syslab::trim_build_cache(root, 0, {new_exe});
  EXPECT(removed >= 1);
  EXPECT(syslab::path_exists(kept_cache));
  EXPECT(!syslab::path_exists(extra_cache));
  syslab::remove_tree(root);
}

void test_resolve_tool_and_perf_manifest() {
  const syslab::ResolvedTool objdump = syslab::resolve_tool("objdump", {"--version"});
  EXPECT(objdump.available);
  EXPECT(!objdump.path.empty());
  EXPECT(objdump.version != "unavailable");
  EXPECT(objdump.version != "binutils");
  const syslab::ResolvedTool missing = syslab::resolve_tool("syslab-no-such-tool-xyz", {"--version"});
  EXPECT(!missing.available);
  EXPECT(missing.path.empty());
  EXPECT(missing.version == "unavailable");
  EXPECT(!missing.reason.empty());

  const syslab::Manifest branch = syslab::load_manifest(
      std::string(SYSLAB_SOURCE_DIR) + "/experiments/cpu/branch-pattern/experiment.yaml");
  EXPECT(branch.perf_events.size() == 1);
  EXPECT(branch.perf_events.front() == "cycles");

  const std::string directory = "/tmp/syslab-manifest-perf";
  syslab::remove_tree(directory);
  syslab::create_directories(directory);
  syslab::write_file(syslab::join_path(directory, "main.cpp"), "int main(){return 0;}\n");
  const std::string yaml =
      "schema_version: 1\n"
      "experiment:\n"
      "  id: AT-009\n"
      "  name: perf-events\n"
      "  module: test.perf\n"
      "  description: \"perf event validation\"\n"
      "  risk: normal\n"
      "platform:\n"
      "  os: linux\n"
      "  architectures:\n"
      "    - x86_64\n"
      "build:\n"
      "  target: exp_at_009\n"
      "  standard: c++20\n"
      "  variants:\n"
      "    - name: debug\n"
      "      build_type: Debug\n"
      "run:\n"
      "  executable: exp_at_009\n"
      "observe:\n"
      "  - performance_counter\n"
      "verify:\n"
      "  checks:\n"
      "    - name: exit\n"
      "      type: exit_code\n"
      "      expected: \"0\"\n"
      "artifacts:\n"
      "  preserve: []\n";
  const std::string path = syslab::join_path(directory, "experiment.yaml");
  syslab::write_file(path, yaml);
  bool rejected = false;
  try {
    syslab::load_manifest(path);
  } catch (const syslab::Error& error) {
    rejected = error.code() == syslab::Error::Code::Validation &&
               std::string(error.what()).find("perf_events") != std::string::npos;
  }
  EXPECT(rejected);
  syslab::write_file(path, yaml + "perf_events:\n  - cycles\n");
  const syslab::Manifest accepted = syslab::load_manifest(path);
  EXPECT(accepted.perf_events.size() == 1);
  syslab::write_file(path, yaml + "perf_events:\n  - cycles name\n");
  bool bad_token = false;
  try {
    syslab::load_manifest(path);
  } catch (const syslab::Error& error) {
    bad_token = error.code() == syslab::Error::Code::Validation;
  }
  EXPECT(bad_token);
  syslab::remove_tree(directory);
}

void test_process_capture_and_timeout() {
  syslab::CommandRequest request;
  request.argv = {"/bin/echo", "hello-capture"};
  request.inherit_environment = true;
  request.timeout_ms = 3000;
  request.stdout_path = "/tmp/syslab-no-such-dir/out.txt";
  const syslab::CommandResult missing = syslab::run_command(request);
  EXPECT(!missing.stdout_capture_error.empty());
  EXPECT(missing.stdout_text.find("hello-capture") != std::string::npos);
  EXPECT(missing.termination == "completed");

  request.stdout_path.clear();
  request.argv = {"/usr/bin/python3", "-c",
                  "import os,time\n"
                  "pid=os.fork()\n"
                  "if pid==0:\n"
                  " os.setsid()\n"
                  " time.sleep(30)\n"
                  "else:\n"
                  " os._exit(0)\n"};
  request.timeout_ms = 400;
  const auto started = std::chrono::steady_clock::now();
  const syslab::CommandResult hung = syslab::run_command(request);
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
  EXPECT(hung.timed_out);
  EXPECT(hung.termination == "timed_out");
  EXPECT(elapsed.count() < 3000);

  request.argv = {"/bin/cat", "/dev/zero"};
  request.timeout_ms = 2000;
  request.output_limit = 128;
  const syslab::CommandResult limited = syslab::run_command(request);
  EXPECT(limited.output_limited);
  EXPECT(limited.termination == "output_limit");
}

void test_metric_summary_and_report() {
  const std::vector<syslab::MetricSample> samples = {
      {"cycles", "1"}, {"cycles", "3"}, {"cycles", "5"}, {"cycles", "7"}};
  const std::vector<syslab::MetricSummary> summary = syslab::summarize_metrics(samples);
  EXPECT(summary.size() == 1);
  EXPECT(summary[0].integer);
  EXPECT(summary[0].count == 4);
  EXPECT(summary[0].minimum == 1);
  EXPECT(summary[0].lower_median == 3);
  EXPECT(summary[0].maximum == 7);
  EXPECT(summary[0].interquartile_range == 4);
  const std::vector<syslab::MetricSummary> mixed =
      syslab::summarize_metrics({{"label", "1"}, {"label", "n/a"}});
  EXPECT(mixed.size() == 1);
  EXPECT(!mixed[0].integer);

  const std::vector<syslab::PerfSample> csv = syslab::parse_perf_csv("# skipped\n123,,cycles,100,100.00\n");
  EXPECT(csv.size() == 1);
  EXPECT(csv[0].event == "cycles");
  EXPECT(csv[0].value == "123");

  syslab::Manifest manifest;
  manifest.experiment.id = "AT-001";
  syslab::ReportInput input;
  input.manifest = &manifest;
  input.variant_build_type = "Debug";
  input.runs.resize(1);
  input.state = syslab::ExperimentState::Passed;
  const std::string report = syslab::render_report(input);
  EXPECT(report.find("调试构建，不作性能结论") != std::string::npos);
  EXPECT(report.find("样本数为 1，不输出离散程度") != std::string::npos);

  const syslab::EnvironmentSnapshot snapshot = syslab::capture_environment("");
  const std::string environment = syslab::environment_to_json(snapshot).dump();
  EXPECT(environment.find("cpufreq_governor") != std::string::npos);
  EXPECT(environment.find("load_average") != std::string::npos);
  EXPECT(environment.find("cpu_affinity") != std::string::npos);
  EXPECT(snapshot.load_average.status == syslab::SampledField::Status::Ok);
  EXPECT(snapshot.cpu_affinity.status == syslab::SampledField::Status::Ok ||
         snapshot.cpu_affinity.status == syslab::SampledField::Status::Failed);
  EXPECT(snapshot.cpufreq_governor.status != syslab::SampledField::Status::Ok ||
         !snapshot.cpufreq_governor.value.empty());
}

void test_course_html() {
  EXPECT(syslab::html_escape("A<B>&\"") == "A&lt;B&gt;&amp;&quot;");
  EXPECT(syslab::html_escape("plain") == "plain");
#ifdef SYSLAB_SOURCE_DIR
  syslab::Registry registry;
  registry.load_tree(syslab::join_path(SYSLAB_SOURCE_DIR, "experiments"));
  const std::vector<syslab::Lesson> lessons = syslab::load_course(SYSLAB_SOURCE_DIR, registry);
  EXPECT(lessons.size() == 94);
  EXPECT(lessons.front().id == "OBJ-001");
  EXPECT(lessons.front().layer == "基础");
  EXPECT(lessons[14].id == "AT-001");
  EXPECT(lessons[14].layer == "基础");
  EXPECT(lessons[15].id == "SYNC-001");
  EXPECT(lessons[15].layer == "进阶");
  bool saw_direction = false;
  for (const syslab::Lesson& lesson : lessons) {
    if (lesson.id == "CPU-003") {
      EXPECT(lesson.layer == "进阶");
    }
    if (lesson.id == "STO-002") {
      EXPECT(lesson.layer == "方向");
      saw_direction = true;
    }
  }
  EXPECT(saw_direction);
  const syslab::LessonCopy copy = syslab::load_briefs(SYSLAB_SOURCE_DIR);
  EXPECT(copy.briefs.at("OBJ-001").term.find("浅拷贝") != std::string::npos);
  EXPECT(copy.briefs.at("OBJ-001").principle.find("深拷贝") != std::string::npos);
  const char* direction_order[] = {"STO-002", "STO-006", "STO-003", "STO-004", "STO-005", "STO-007",
                                   "SRV-002", "SRV-005", "SRV-003", "SRV-004", "SRV-006", "CTR-001",
                                   "CTR-002", "CTR-005", "CTR-006", "CTR-003", "CTR-004", "SEC-001",
                                   "SEC-002", "SEC-003", "SEC-004", "AV-001", "AV-004", "AV-002", "AV-003",
                                   "AG-001", "AG-004", "AG-005", "AG-002", "AG-003"};
  std::size_t direction_at = 0;
  for (const syslab::Lesson& lesson : lessons) {
    if (lesson.layer != "方向") {
      continue;
    }
    if (direction_at < 30) {
      EXPECT(lesson.id == direction_order[direction_at]);
    }
    ++direction_at;
  }
  EXPECT(direction_at == 30);
  EXPECT(registry.get("CPU-002").prerequisites.size() == 1);
  EXPECT(registry.get("CPU-002").prerequisites.front() == "CPU-001");
  const syslab::Lesson* cpu001 = syslab::find_lesson(lessons, "CPU-001");
  EXPECT(cpu001 != nullptr && cpu001->title.find("补码") != std::string::npos);
  const std::pair<std::string, std::string> cause = syslab::split_cause("甲。乙。此后丙。丁。");
  EXPECT(cause.first == "甲。乙。");
  EXPECT(cause.second == "此后丙。丁。");
  const std::pair<std::string, std::string> plain = syslab::split_cause("甲。乙。丙。丁。");
  EXPECT(plain.second.empty());
  EXPECT(syslab::loosen_background("甲。乙。丙。丁。") == "甲。乙。\n\n丙。丁。");
#else
  EXPECT(false && "SYSLAB_SOURCE_DIR is not defined");
#endif
}

void test_parsers() {
  const syslab::Json object = syslab::Json::parse("{\"path\":\"a\\\\b\\\"c\",\"n\":9223372036854775807}");
  const std::string dumped = object.dump();
  EXPECT(dumped.find("9223372036854775807") != std::string::npos);
  bool bad_json = false;
  try {
    syslab::Json::parse("{");
  } catch (const syslab::Error& error) {
    bad_json = error.code() == syslab::Error::Code::Parse;
  }
  EXPECT(bad_json);

  bool tab = false;
  try {
    syslab::Yaml::parse("key:\n\tchild: 1\n", "memory");
  } catch (const syslab::Error& error) {
    tab = error.code() == syslab::Error::Code::Parse &&
          std::string(error.what()).find("tabs are not allowed") != std::string::npos;
  }
  EXPECT(tab);
  bool flow = false;
  try {
    syslab::Yaml::parse("key: {a: 1}\n", "memory");
  } catch (const syslab::Error& error) {
    flow = error.code() == syslab::Error::Code::Parse;
  }
  EXPECT(flow);
}

void test_mpmc_linearizability() {
  struct HistoryOp {
    bool push = false;
    int value = 0;
    bool ok = false;
    std::uint64_t start = 0;
    std::uint64_t end = 0;
  };
  syslab::MpmcQueue<int, 4> queue;
  std::mutex log_mutex;
  std::vector<HistoryOp> history;
  auto stamp = []() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
  };
  auto record = [&](HistoryOp op) {
    std::lock_guard<std::mutex> guard(log_mutex);
    history.push_back(op);
  };
  std::thread first([&]() {
    for (int value = 1; value <= 2; ++value) {
      HistoryOp op;
      op.push = true;
      op.value = value;
      op.start = stamp();
      op.ok = queue.push(value);
      op.end = stamp();
      record(op);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (int count = 0; count < 2; ++count) {
      HistoryOp op;
      op.push = false;
      op.start = stamp();
      op.ok = queue.pop(op.value);
      op.end = stamp();
      record(op);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
  std::thread second([&]() {
    for (int value = 3; value <= 4; ++value) {
      HistoryOp op;
      op.push = true;
      op.value = value;
      op.start = stamp();
      op.ok = queue.push(value);
      op.end = stamp();
      record(op);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (int count = 0; count < 2; ++count) {
      HistoryOp op;
      op.push = false;
      op.start = stamp();
      op.ok = queue.pop(op.value);
      op.end = stamp();
      record(op);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
  first.join();
  second.join();
  EXPECT(history.size() == 8);

  const auto sequential = [](const std::vector<const HistoryOp*>& order) {
    std::deque<int> cells;
    for (const HistoryOp* op : order) {
      if (op->push) {
        if (op->ok) {
          if (cells.size() == 4) {
            return false;
          }
          cells.push_back(op->value);
        } else if (cells.size() != 4) {
          return false;
        }
      } else if (op->ok) {
        if (cells.empty() || cells.front() != op->value) {
          return false;
        }
        cells.pop_front();
      } else if (!cells.empty()) {
        return false;
      }
    }
    return true;
  };
  const auto search = [&](auto&& self, std::vector<char>& used, std::vector<const HistoryOp*>& order) -> bool {
    if (order.size() == history.size()) {
      return true;
    }
    for (std::size_t index = 0; index < history.size(); ++index) {
      if (used[index] != 0) {
        continue;
      }
      bool ready = true;
      for (std::size_t other = 0; other < history.size(); ++other) {
        if (used[other] == 0 && history[other].end < history[index].start) {
          ready = false;
          break;
        }
      }
      if (!ready) {
        continue;
      }
      used[index] = 1;
      order.push_back(&history[index]);
      if (sequential(order) && self(self, used, order)) {
        return true;
      }
      order.pop_back();
      used[index] = 0;
    }
    return false;
  };
  std::vector<char> used(history.size(), 0);
  std::vector<const HistoryOp*> order;
  EXPECT(search(search, used, order));
}

void test_stress() {
  const char* text = std::getenv("SYSLAB_STRESS_ITERATIONS");
  int iterations = 1000;
  if (text != nullptr && text[0] != '\0') {
    iterations = std::atoi(text);
  }
  if (iterations < 1) {
    iterations = 1;
  }
  syslab::MpmcQueue<int, 1024> queue;
  std::atomic<int> produced {0};
  std::atomic<int> consumed {0};
  std::mutex values_mutex;
  std::vector<int> values;
  std::vector<std::thread> threads;
  for (int worker = 0; worker < 4; ++worker) {
    threads.emplace_back([&, worker]() {
      for (int index = worker; index < iterations; index += 4) {
        while (!queue.push(index)) {
        }
        produced.fetch_add(1, std::memory_order_release);
      }
    });
  }
  for (int worker = 0; worker < 4; ++worker) {
    threads.emplace_back([&]() {
      while (consumed.load(std::memory_order_acquire) < iterations) {
        int value = 0;
        if (!queue.pop(value)) {
          continue;
        }
        {
          std::lock_guard<std::mutex> guard(values_mutex);
          values.push_back(value);
        }
        consumed.fetch_add(1, std::memory_order_release);
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  EXPECT(static_cast<int>(values.size()) == iterations);
  std::sort(values.begin(), values.end());
  bool matched = static_cast<int>(values.size()) == iterations;
  for (int index = 0; matched && index < iterations; ++index) {
    if (values[static_cast<std::size_t>(index)] != index) {
      matched = false;
    }
  }
  EXPECT(matched);
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

int main(int argc, char** argv) {
  struct NamedTest {
    const char* name;
    void (*function)();
    bool stress;
  };
  const NamedTest tests[] = {
      {"sha_and_yaml", test_sha_and_yaml, false},
      {"lifecycle", test_lifecycle, false},
      {"arena_and_pool", test_arena_and_pool, false},
      {"freelist_and_buddy", test_freelist_and_buddy, false},
      {"size_and_cache", test_size_and_cache, false},
      {"locks", test_locks, false},
      {"queues_and_stack", test_queues_and_stack, false},
      {"epoch", test_epoch, false},
      {"fiber_coroutine_and_steal", test_fiber_coroutine_and_steal, false},
      {"dependencies_and_digest", test_dependencies_and_digest, false},
      {"protocol", test_protocol, false},
      {"clean_artifacts", test_clean_artifacts, false},
      {"catalog", test_catalog, false},
      {"resolve_tool_and_perf_manifest", test_resolve_tool_and_perf_manifest, false},
      {"process_capture_and_timeout", test_process_capture_and_timeout, false},
      {"metric_summary_and_report", test_metric_summary_and_report, false},
      {"course_html", test_course_html, false},
      {"parsers", test_parsers, false},
      {"mpmc_linearizability", test_mpmc_linearizability, false},
      {"runner", test_runner, false},
      {"stress", test_stress, true},
  };
  const std::string selected = argc > 1 ? argv[1] : "";
  int ran = 0;
  try {
    for (const NamedTest& test : tests) {
      if (!selected.empty() && selected != test.name) {
        continue;
      }
      if (selected.empty() && test.stress) {
        continue;
      }
      test.function();
      ++ran;
    }
  } catch (const std::exception& error) {
    std::cerr << "uncaught " << error.what() << "\n";
    return 1;
  }
  if (ran == 0) {
    std::cerr << "unknown test " << selected << "\n";
    return 1;
  }
  if (g_failed != 0) {
    std::cerr << g_failed << " assertion(s) failed\n";
    return 1;
  }
  std::cout << "unit tests passed\n";
  return 0;
}
