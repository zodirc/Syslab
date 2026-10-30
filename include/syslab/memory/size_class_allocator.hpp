#pragma once

#include "syslab/memory/allocator.hpp"
#include "syslab/memory/pool_allocator.hpp"

#include <memory>
#include <vector>

namespace syslab {

/**
 * @brief Size-class allocator backed by fixed pools and direct mappings.
 * 由固定池和直接映射支撑的大小类分配器。
 *
 * Requests up to 4096 bytes select a size class. Larger requests receive a
 * dedicated mapping. The allocator is not thread-safe. Counters describe the
 * user-visible allocations. Pool-internal reserved bytes are included in
 * reserved_bytes. This is not a production allocator.
 * 不超过 4096 字节的请求选择一个大小类。更大的请求得到单独映射。该分配器不是线程安全的。计数器描述用户可见的分配。池内部预留字节计入 reserved_bytes。这不是生产分配器。
 */
class SizeClassAllocator final : public IAllocator {
 public:
  /**
   * @brief Creates empty size-class pools. Pools grow on first use.
   * 创建空的大小类池。池在首次使用时增长。
   * @param blocks_per_class Number of blocks created in a class pool.
   * 一个类池中创建的块数。
   */
  explicit SizeClassAllocator(std::size_t blocks_per_class = 64);
  ~SizeClassAllocator() override;

  void* allocate(const AllocationRequest& request) override;
  void deallocate(void* pointer, std::size_t size, std::size_t alignment) override;
  AllocationStats statistics() const override;

 private:
  struct Large {
    void* mapping = nullptr;
    std::size_t mapping_size = 0;
    void* pointer = nullptr;
    std::size_t size = 0;
    std::size_t alignment = 0;
  };

  std::size_t blocks_per_class_;
  std::vector<std::unique_ptr<PoolAllocator>> classes_;
  std::vector<std::size_t> class_sizes_;
  std::vector<Large> large_;
  AllocationStats stats_{};
};

}  // namespace syslab
