#pragma once

#include <cstddef>
#include <cstdint>

namespace syslab {

/**
 * @brief Bitmask of AllocationStats counters that contain measurements.
 * AllocationStats 中已测量计数器的位掩码。
 *
 * A counter whose bit is clear is not a measurement. Callers must ignore the
 * numeric field in that case. Zero is a real measurement only when the bit is set.
 * 某一位未置位就不是测量。调用方必须忽略对应的数值字段。只有该位置位时，零才是真实测量。
 */
enum class StatField : std::uint32_t {
  Reserved = 1u << 0,
  Committed = 1u << 1,
  Allocated = 1u << 2,
  Free = 1u << 3,
  AllocationCount = 1u << 4,
  DeallocationCount = 1u << 5,
  PeakAllocated = 1u << 6
};

/**
 * @brief Request passed to a teaching allocator.
 * 交给教学分配器的请求。
 *
 * size must be non-zero. alignment must be a power of two. tag is recorded
 * and is not interpreted by the allocator.
 * size 必须非零。alignment 必须是 2 的幂。tag 只被记录，分配器不解释它。
 */
struct AllocationRequest {
  std::size_t size = 0;
  std::size_t alignment = alignof(std::max_align_t);
  std::uint32_t tag = 0;
};

/**
 * @brief Counters reported by a teaching allocator.
 * 教学分配器报告的计数器。
 *
 * These allocators are single-threaded unless the concrete class documents
 * otherwise. They are not substitutes for the global allocator.
 * 除非具体类另有说明，这些分配器是单线程的。它们不能代替全局分配器。
 */
struct AllocationStats {
  std::size_t reserved_bytes = 0;
  std::size_t committed_bytes = 0;
  std::size_t allocated_bytes = 0;
  std::size_t free_bytes = 0;
  std::size_t allocation_count = 0;
  std::size_t deallocation_count = 0;
  std::size_t peak_allocated_bytes = 0;
  std::uint32_t available_fields = 0;

  /**
   * @brief Reports whether a counter was measured.
   * 报告某个计数器是否被测量过。
   * @param field Counter selector.
   * 计数器选择项。
   * @return True when the corresponding numeric field is meaningful.
   * 对应数值字段有意义时为真。
   */
  bool has(StatField field) const noexcept {
    return (available_fields & static_cast<std::uint32_t>(field)) != 0;
  }
};

/**
 * @brief Teaching allocator interface.
 * 教学分配器接口。
 *
 * allocate returns null when the backing store cannot satisfy the request.
 * deallocate throws on a foreign pointer, a double free, or a damaged canary.
 * A null pointer is ignored by deallocate.
 * 后备存储无法满足请求时 allocate 返回空。外来指针、重复释放或损坏的 canary 会使 deallocate 抛出异常。空指针会被忽略。
 */
class IAllocator {
 public:
  virtual ~IAllocator() = default;

  /**
   * @brief Allocates a block.
   * 分配一块内存。
   * @param request Size, alignment, and tag.
   * 大小、对齐和标记。
   * @return Aligned pointer, or null when the store is exhausted.
   * 对齐后的指针。存储耗尽时为空。
   * @throws Error InvalidArgument when size or alignment is illegal.
   * 大小或对齐非法时抛出 Error InvalidArgument。
   */
  virtual void* allocate(const AllocationRequest& request) = 0;

  /**
   * @brief Releases a block returned by allocate.
   * 释放 allocate 返回的块。
   * @param pointer Block pointer, or null.
   * 块指针，或空。
   * @param size Size passed to allocate.
   * 传给 allocate 的大小。
   * @param alignment Alignment passed to allocate.
   * 传给 allocate 的对齐。
   * @throws Error Corruption when the pointer, size, canary, or free state is inconsistent.
   * 指针、大小、canary 或空闲状态不一致时抛出 Error Corruption。
   */
  virtual void deallocate(void* pointer, std::size_t size, std::size_t alignment) = 0;

  /**
   * @brief Returns a snapshot of allocator counters.
   * 返回分配器计数器的快照。
   * @return Counters. Unavailable fields are identified by available_fields.
   * 计数器。不可用字段由 available_fields 标明。
   */
  virtual AllocationStats statistics() const = 0;
};

}  // namespace syslab
