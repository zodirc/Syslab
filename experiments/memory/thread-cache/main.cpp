#include "experiments/common/check.hpp"
#include "syslab/memory/size_class_allocator.hpp"
#include "syslab/memory/thread_cache_allocator.hpp"
#include <memory>
#include <thread>
#include <vector>
// 应用：一个线程释放的内存没有回到分配它的缓存，或者一个尺寸的空闲块被发给了另一个尺寸的请求。
// 原理：线程缓存的箱按精确请求大小存放。四个线程各自分配并释放 40 个 32 字节块之后，统计中的 allocated_bytes 为 0。缓存中的 20 字节块不会返回给 32 字节请求。
int main() {
  syslab::ThreadCacheAllocator cache(std::make_unique<syslab::SizeClassAllocator>(128));
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&cache]() {
      void* local[40];
      for (int item = 0; item < 40; ++item) {
        local[item] = cache.allocate({32, 8, 0});
        if (local[item] == nullptr) std::terminate();
      }
      for (void* pointer : local) cache.deallocate(pointer, 32, 8);
    });
  }
  for (std::thread& thread : threads) thread.join();
  const syslab::AllocationStats stats = cache.statistics();
  if (!stats.has(syslab::StatField::Allocated) || stats.allocated_bytes != 0) {
    syslab_step("释放之后已分配字节不是 0。");
    return syslab_fail("stats");
  }
  syslab_note("OBSERVED", "bins keep an exact request size; a cached 20-byte block is not returned for 32 bytes");
    syslab_step("四个线程分配并释放 32 字节块之后，已分配字节为 0。");
  return syslab_pass();
}
