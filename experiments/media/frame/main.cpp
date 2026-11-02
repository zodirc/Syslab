#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <vector>

// 现象：像素 (1,1) 写成 11 22 33 后，同一行的 (2,1) 和上一行的 (1,0) 仍是 0。
// 原理：RGB24 紧密排列时，像素 (x,y) 的偏移是 (y * width + x) * 3。
int main() {
  constexpr int kWidth = 4;
  constexpr int kHeight = 2;
  constexpr int kBytesPerPixel = 3;
  std::vector<unsigned char> frame(static_cast<std::size_t>(kWidth * kHeight * kBytesPerPixel), 0);
  const int pixel = (1 * kWidth + 1) * kBytesPerPixel;
  frame[static_cast<std::size_t>(pixel)] = 0x11;
  frame[static_cast<std::size_t>(pixel + 1)] = 0x22;
  frame[static_cast<std::size_t>(pixel + 2)] = 0x33;
  const int fd = ::open("frame.rgb", O_CREAT | O_TRUNC | O_RDWR, 0644);
  if (fd < 0 || ::write(fd, frame.data(), frame.size()) != static_cast<ssize_t>(frame.size())) {
    if (fd >= 0) {
      ::close(fd);
    }
    syslab_step("帧没有写完。");
    return syslab_fail("write");
  }
  unsigned char got[3] = {};
  unsigned char right[3] = {1, 1, 1};
  unsigned char above[3] = {1, 1, 1};
  const int right_at = (1 * kWidth + 2) * kBytesPerPixel;
  const int above_at = (0 * kWidth + 1) * kBytesPerPixel;
  if (::pread(fd, got, 3, pixel) != 3 || std::memcmp(got, "\x11\x22\x33", 3) != 0 ||
      ::pread(fd, right, 3, right_at) != 3 || std::memcmp(right, "\0\0\0", 3) != 0 ||
      ::pread(fd, above, 3, above_at) != 3 || std::memcmp(above, "\0\0\0", 3) != 0) {
    ::close(fd);
    syslab_step("像素偏移上的字节和相邻像素不一致。");
    return syslab_fail("pixel");
  }
  ::close(fd);
  syslab_metric("offset", pixel);
  syslab_note("OS", "packed RGB24 addresses pixel x y at (y * width + x) * 3");
  syslab_step("像素 (1,1) 是 11 22 33。像素 (2,1) 和 (1,0) 仍是 0。");
  return syslab_pass();
}
