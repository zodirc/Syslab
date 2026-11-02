#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <vector>

// 应用：按页读回的内容串到相邻页时，先核对页号和页长的乘积。
// 原理：页号乘以页长是文件偏移。写入一个页不改变另一个页。
int main() {
  const long page = ::sysconf(_SC_PAGESIZE);
  if (page <= 0) {
    syslab_step("没有得到页长。");
    return syslab_fail("pagesize");
  }
  const std::size_t page_size = static_cast<std::size_t>(page);
  const int fd = ::open("pages.bin", O_CREAT | O_TRUNC | O_RDWR, 0644);
  if (fd < 0) {
    syslab_step("页文件没有打开。");
    return syslab_fail("open");
  }
  std::vector<unsigned char> page0(page_size, 0xA1);
  std::vector<unsigned char> page1(page_size, 0xB2);
  if (::pwrite(fd, page0.data(), page_size, 0) != static_cast<ssize_t>(page_size) ||
      ::pwrite(fd, page1.data(), page_size, static_cast<off_t>(page_size)) != static_cast<ssize_t>(page_size)) {
    syslab_step("两页没有写完。");
    return syslab_fail("write");
  }
  std::vector<unsigned char> read1(page_size, 0);
  std::vector<unsigned char> read0(page_size, 0);
  const off_t page_one = static_cast<off_t>(page_size);
  if (::pread(fd, read1.data(), page_size, page_one) != static_cast<ssize_t>(page_size) ||
      ::pread(fd, read0.data(), page_size, 0) != static_cast<ssize_t>(page_size) ||
      std::memcmp(read1.data(), page1.data(), page_size) != 0 ||
      std::memcmp(read0.data(), page0.data(), page_size) != 0) {
    syslab_step("按页号读回的字节和写入的页不一致。");
    return syslab_fail("page");
  }
  ::close(fd);
  syslab_metric("page_size", page);
  syslab_metric("page_number", 1);
  syslab_note("OS", "page number times page size is the file offset of that page");
  syslab_step("页号 1 乘以页长得到的区间读回 0xB2，页号 0 仍是 0xA1。");
  return syslab_pass();
}
