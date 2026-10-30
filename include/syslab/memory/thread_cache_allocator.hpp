#pragma once

#include "syslab/memory/allocator.hpp"

#include <memory>

namespace syslab {

/**
 * @brief Thread-local cache in front of a single-threaded upstream allocator.
 * 放在单线程上游分配器前面的线程局部缓存。
 *
 * Same-thread deallocation returns a block to that thread's cache. A free from
 * another thread returns the block to the upstream allocator under the cache
 * lock. The upstream allocator is not used concurrently. Statistics describe
 * bytes currently handed to callers. Cached free blocks are included in
 * free_bytes. Destruction waits, through shared ownership, until thread caches
 * have flushed. Threads must not call the allocator during its destructor.
 * This is not a production allocator.
 * 同一线程释放时，块回到该线程的缓存。其他线程释放时，在缓存锁下把块还给上游。上游分配器不会被并发使用。统计描述当前交给调用者的字节。缓存中的空闲块计入 free_bytes。析构通过共享所有权等待线程缓存刷完。线程不得在析构期间调用分配器。这不是生产分配器。
 */
class ThreadCacheAllocator final : public IAllocator {
 public:
  /**
   * @brief Takes ownership of the upstream allocator.
   * 取得上游分配器的所有权。
   * @param upstream Allocator used to refill caches. It must not be shared.
   * 用来补充缓存的分配器。它不能被共享。
   * @throws Error InvalidArgument when upstream is null.
   * upstream 为空时抛出 Error InvalidArgument。
   */
  explicit ThreadCacheAllocator(std::unique_ptr<IAllocator> upstream);
  ~ThreadCacheAllocator() override;

  void* allocate(const AllocationRequest& request) override;
  void deallocate(void* pointer, std::size_t size, std::size_t alignment) override;
  AllocationStats statistics() const override;

 private:
  struct State;
  friend struct ThreadCacheAccess;
  std::shared_ptr<State> state_;
};

struct ThreadCacheAccess;

}  // namespace syslab
