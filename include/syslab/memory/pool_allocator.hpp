#pragma once

#include "syslab/memory/allocator.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace syslab {

/**
 * @brief Fixed-size pool over one anonymous mapping.
 * 建在一块匿名映射上的固定块池。
 *
 * Every successful allocation has the same user capacity. A request larger
 * than that capacity, or with a stricter alignment than the pool, returns null.
 * Free blocks form an index free list. The allocator detects a double free and
 * a damaged end canary. It is not thread-safe and is not a production allocator.
 * 每次成功分配的用户容量相同。请求大于该容量，或对齐比池更严时，返回空。空闲块组成下标空闲链表。分配器能发现重复释放和损坏的尾部 canary。它不是线程安全的，也不是生产分配器。
 */
class PoolAllocator final : public IAllocator {
 public:
  /**
   * @brief Creates a pool of identical blocks.
   * 创建由相同块组成的池。
   * @param block_size User capacity of one block.
   * 一块的用户容量。
   * @param block_count Number of blocks.
   * 块数。
   * @param alignment Alignment guaranteed for every block.
   * 每一块保证的对齐。
   * @throws Error InvalidArgument when a parameter is zero or the alignment is illegal.
   * 参数为零或对齐非法时抛出 Error InvalidArgument。
   * @throws Error Io when the mapping cannot be created.
   * 无法创建映射时抛出 Error Io。
   */
  PoolAllocator(std::size_t block_size, std::size_t block_count, std::size_t alignment);
  ~PoolAllocator() override;

  void* allocate(const AllocationRequest& request) override;
  void deallocate(void* pointer, std::size_t size, std::size_t alignment) override;
  AllocationStats statistics() const override;

 private:
  struct Block {
    std::uint32_t next = 0;
    bool live = false;
    bool in_list = false;
  };

  void* base_ = nullptr;
  std::size_t mapping_size_ = 0;
  std::size_t block_size_ = 0;
  std::size_t alignment_ = 0;
  std::size_t stride_ = 0;
  std::size_t user_delta_ = 0;
  std::uint32_t free_head_ = 0;
  std::size_t free_count_ = 0;
  std::vector<Block> blocks_;
  AllocationStats stats_{};
};

}  // namespace syslab
