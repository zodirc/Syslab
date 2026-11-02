#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <vector>

// 现象：行跨距 16 时，像素 (0,1) 在偏移 16。按紧密排列会落到的偏移 12 仍是 0。
// 原理：下一行从 stride 开始，不从 width * 3 开始。stride 大于一行像素占用的字节数。
int main() {
  constexpr int kWidth = 4;
  constexpr int kBytesPerPixel = 3;
  constexpr int kStride = 16;
  constexpr int kHeight = 2;
  std::vector<unsigned char> frame(static_cast<std::size_t>(kStride * kHeight), 0);
  frame[static_cast<std::size_t>(kStride)] = 0xAB;
  const int fd = ::open("stride.rgb", O_CREAT | O_TRUNC | O_RDWR, 0644);
  if (fd < 0 || ::write(fd, frame.data(), frame.size()) != static_cast<ssize_t>(frame.size())) {
    if (fd >= 0) {
      ::close(fd);
    }
    syslab_step("带跨距的帧没有写完。");
    return syslab_fail("write");
  }
  unsigned char at_stride = 0;
  unsigned char packed = 1;
  const int packed_next_row = kWidth * kBytesPerPixel;
  if (::pread(fd, &at_stride, 1, kStride) != 1 || at_stride != 0xAB ||
      ::pread(fd, &packed, 1, packed_next_row) != 1 || packed != 0) {
    ::close(fd);
    syslab_step("下一行没有落在 stride 上。");
    return syslab_fail("stride");
  }
  ::close(fd);
  syslab_metric("stride", kStride);
  syslab_metric("packed_offset", packed_next_row);
  syslab_note("OS", "a padded row starts at stride, which can be greater than width times bytes per pixel");
  syslab_step("像素 (0,1) 在偏移 16，字节是 AB。偏移 12 仍是 0。");
  return syslab_pass();
}
