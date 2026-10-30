#pragma once

#include "syslab/memory/allocator.hpp"

#include <cstddef>
#include <vector>

namespace syslab {

/**
 * @brief First-fit free-list allocator with adjacent coalescing.
 * 带相邻合并的首次适配空闲链表分配器。
 *
 * Span metadata is kept outside the mapped heap. Each live user block also
 * carries an in-band header and end canary so an overflow or a double free is
 * detectable. free_bytes is the sum of free spans and is therefore a measured
 * value. The allocator is not thread-safe and is not a production allocator.
 * 区间元数据放在映射堆之外。每个存活的用户块还带有带内头部和尾部 canary，以便发现越界或重复释放。free_bytes 是空闲区间之和，因此是测量值。该分配器不是线程安全的，也不是生产分配器。
 */
class FreeListAllocator final : public IAllocator {
 public:
  /**
   * @brief Creates a heap of the requested size.
   * 按请求大小创建堆。
   * @param capacity Mapped heap size.
   * 映射堆的大小。
   * @throws Error InvalidArgument when capacity is below the minimum span.
   * 容量低于最小区间时抛出 Error InvalidArgument。
   * @throws Error Io when the mapping cannot be created.
   * 无法创建映射时抛出 Error Io。
   */
  explicit FreeListAllocator(std::size_t capacity);
  ~FreeListAllocator() override;

  void* allocate(const AllocationRequest& request) override;
  void deallocate(void* pointer, std::size_t size, std::size_t alignment) override;
  AllocationStats statistics() const override;

 private:
  struct Span {
    std::size_t offset = 0;
    std::size_t size = 0;
    std::size_t user_size = 0;
    std::size_t alignment = 0;
    void* pointer = nullptr;
    bool free = true;
  };

  void* base_ = nullptr;
  std::size_t capacity_ = 0;
  std::vector<Span> spans_;
  AllocationStats stats_{};

  void recompute_free();
};

}  // namespace syslab
