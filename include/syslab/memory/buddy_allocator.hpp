#pragma once

#include "syslab/memory/allocator.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace syslab {

/**
 * @brief Power-of-two buddy allocator.
 * 2 的幂伙伴分配器。
 *
 * The mapping size is rounded up to a power of two. Blocks split and merge
 * with their buddy. An in-band canary detects overflow, and the buddy metadata
 * detects a double free. The allocator is not thread-safe and is not a
 * production allocator.
 * 映射大小向上取整到 2 的幂。块与它的伙伴分裂和合并。带内 canary 发现越界，伙伴元数据发现重复释放。该分配器不是线程安全的，也不是生产分配器。
 */
class BuddyAllocator final : public IAllocator {
 public:
  /**
   * @brief Creates a buddy heap.
   * 创建一个伙伴堆。
   * @param capacity Requested size. The mapping is rounded up to a power of two.
   * 请求的大小。映射会向上取整到 2 的幂。
   * @throws Error InvalidArgument when capacity is zero.
   * 容量为零时抛出 Error InvalidArgument。
   * @throws Error Io when the mapping cannot be created.
   * 无法创建映射时抛出 Error Io。
   */
  explicit BuddyAllocator(std::size_t capacity);
  ~BuddyAllocator() override;

  void* allocate(const AllocationRequest& request) override;
  void deallocate(void* pointer, std::size_t size, std::size_t alignment) override;
  AllocationStats statistics() const override;

 private:
  struct Meta {
    int order = 0;
    bool free = false;
    bool present = false;
    std::size_t user_size = 0;
    std::size_t alignment = 0;
  };

  void* base_ = nullptr;
  std::size_t capacity_ = 0;
  std::size_t min_block_ = 64;
  int max_order_ = 0;
  std::vector<std::vector<std::size_t>> free_lists_;
  std::vector<Meta> meta_;
  AllocationStats stats_{};
};

}  // namespace syslab
