#pragma once

#include "syslab/memory/allocator.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace syslab {

/**
 * @brief Linear allocator over one anonymous mapping.
 * 建在一块匿名映射上的线性分配器。
 *
 * Individual deallocate checks the canary and detects a double free. The byte
 * range is reusable only when the freed block is the most recent bump. Other
 * frees leave a hole until reset. free_bytes counts bytes that have not been
 * bumped, including space reclaimed by a LIFO free. This allocator is not
 * thread-safe and is not a production allocator.
 * 单次 deallocate 检查 canary 并发现重复释放。只有被释放的块是最近一次 bump 时，那段字节才能复用。其他释放留下空洞，直到 reset。free_bytes 统计尚未 bump 的字节，包括后进先出释放收回的空间。该分配器不是线程安全的，也不是生产分配器。
 */
class ArenaAllocator final : public IAllocator {
 public:
  /**
   * @brief Reserves a mapping for bump allocation.
   * 为 bump 分配保留一块映射。
   * @param capacity Requested mapping size in bytes.
   * 请求的映射字节数。
   * @throws Error InvalidArgument when capacity is zero.
   * 容量为零时抛出 Error InvalidArgument。
   * @throws Error Io when the mapping cannot be created.
   * 无法创建映射时抛出 Error Io。
   */
  explicit ArenaAllocator(std::size_t capacity);
  ~ArenaAllocator() override;

  void* allocate(const AllocationRequest& request) override;
  void deallocate(void* pointer, std::size_t size, std::size_t alignment) override;
  AllocationStats statistics() const override;

  /**
   * @brief Drops every live allocation and rewinds the bump pointer.
   * 丢弃全部分配并把 bump 指针拨回起点。
   *
   * Outstanding pointers become invalid. This is not deallocate.
   * 尚未归还的指针变为无效。这不是 deallocate。
   */
  void reset();

 private:
  struct Record {
    void* pointer = nullptr;
    std::size_t size = 0;
    std::size_t alignment = 0;
    std::size_t begin = 0;
    std::size_t end = 0;
    bool live = false;
  };

  void* base_ = nullptr;
  std::size_t capacity_ = 0;
  std::size_t offset_ = 0;
  AllocationStats stats_{};
  std::vector<Record> records_;
};

}  // namespace syslab
