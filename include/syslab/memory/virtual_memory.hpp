#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace syslab {

/**
 * @brief Anonymous mapping owned by one object.
 * 由一个对象拥有的匿名映射。
 *
 * The destructor releases the mapping. Move transfers ownership. Copy is rejected.
 * 析构时释放映射。移动转移所有权。禁止拷贝。
 */
class MappedRegion {
 public:
  /**
   * @brief Creates an empty region.
   * 创建一个空区域。
   */
  MappedRegion() = default;

  /**
   * @brief Maps an anonymous private region.
   * 映射一块匿名私有区域。
   * @param bytes Size rounded up to the page size. Zero is rejected.
   * 向上取整到页大小的长度。零会被拒绝。
   * @param readable Whether the pages are readable.
   * 页是否可读。
   * @param writable Whether the pages are writable.
   * 页是否可写。
   * @throws Error InvalidArgument when bytes is zero.
   * 字节数为零时抛出 Error InvalidArgument。
   * @throws Error Io when mmap fails.
   * mmap 失败时抛出 Error Io。
   */
  MappedRegion(std::size_t bytes, bool readable = true, bool writable = true);

  ~MappedRegion();
  MappedRegion(const MappedRegion&) = delete;
  MappedRegion& operator=(const MappedRegion&) = delete;
  MappedRegion(MappedRegion&& other) noexcept;
  MappedRegion& operator=(MappedRegion&& other) noexcept;

  /**
   * @brief Returns the mapping base.
   * 返回映射基址。
   * @return Base pointer, or null when the region is empty.
   * 基址指针。区域为空时是空指针。
   */
  void* data() const noexcept { return data_; }

  /**
   * @brief Returns the mapped size in bytes.
   * 返回映射的字节数。
   * @return Mapped byte count, including page rounding.
   * 映射字节数，含页取整。
   */
  std::size_t size() const noexcept { return size_; }

  /**
   * @brief Releases the mapping.
   * 释放映射。
   */
  void reset() noexcept;

  /**
   * @brief Detaches the mapping without releasing it.
   * 分离映射但不释放它。
   * @return Previous base pointer. The caller must munmap it with size().
   * 原先的基址。调用者必须用 size() 对它 munmap。
   */
  void* release() noexcept;

 private:
  void* data_ = nullptr;
  std::size_t size_ = 0;
};

/**
 * @brief Changes protection on an existing mapping.
 * 改变已有映射的保护。
 * @param data Mapping base. Must be page aligned.
 * 映射基址。必须页对齐。
 * @param bytes Length. Must cover whole pages.
 * 长度。必须覆盖整页。
 * @param readable Read permission.
 * 读权限。
 * @param writable Write permission.
 * 写权限。
 * @throws Error Io when mprotect fails.
 * mprotect 失败时抛出 Error Io。
 */
void protect_region(void* data, std::size_t bytes, bool readable, bool writable);

/**
 * @brief Counts resident pages in a mapping with mincore.
 * 用 mincore 统计映射中的驻留页。
 * @param data Mapping base.
 * 映射基址。
 * @param bytes Mapping length.
 * 映射长度。
 * @param resident_pages Receives the resident page count when the call succeeds.
 * 调用成功时接收驻留页数。
 * @return False when mincore is unavailable or fails. resident_pages is unchanged then.
 * mincore 不可用或失败时为假。此时 resident_pages 不变。
 */
bool count_resident_pages(void* data, std::size_t bytes, std::size_t* resident_pages);

/**
 * @brief Reads the process page size.
 * 读取进程页大小。
 * @return Page size in bytes.
 * 页大小，单位字节。
 * @throws Error Io when the page size cannot be read.
 * 无法读取页大小时抛出 Error Io。
 */
std::size_t system_page_size();

/**
 * @brief Minor and major fault counters from getrusage.
 * 来自 getrusage 的次缺页和主缺页计数。
 */
struct FaultCounters {
  long minor = 0;
  long major = 0;
};

/**
 * @brief Reads fault counters for the calling process.
 * 读取调用进程的缺页计数。
 * @return Counters from getrusage(RUSAGE_SELF).
 * getrusage(RUSAGE_SELF) 的计数。
 * @throws Error Io when getrusage fails.
 * getrusage 失败时抛出 Error Io。
 */
FaultCounters read_fault_counters();

}  // namespace syslab
