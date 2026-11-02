#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>

// 现象：平面布局里，帧号 2 的左声道是 1000，右声道是 -3。帧号 1 的左声道仍是 0。
// 原理：左声道平面在前，右声道平面在后。帧号 i 的左声道偏移是 i * 2，右声道偏移是 8 + i * 2。
int main() {
  std::int16_t left[4] = {};
  std::int16_t right[4] = {};
  left[2] = 1000;
  right[2] = -3;
  const int fd = ::open("planar.pcm", O_CREAT | O_TRUNC | O_RDWR, 0644);
  if (fd < 0 || ::write(fd, left, sizeof(left)) != static_cast<ssize_t>(sizeof(left)) ||
      ::write(fd, right, sizeof(right)) != static_cast<ssize_t>(sizeof(right))) {
    if (fd >= 0) {
      ::close(fd);
    }
    syslab_step("平面采样没有写完。");
    return syslab_fail("write");
  }
  std::int16_t left_sample = 1;
  std::int16_t right_sample = 1;
  std::int16_t earlier = 1;
  if (::pread(fd, &left_sample, sizeof(left_sample), 4) != static_cast<ssize_t>(sizeof(left_sample)) ||
      left_sample != 1000 ||
      ::pread(fd, &right_sample, sizeof(right_sample), 12) != static_cast<ssize_t>(sizeof(right_sample)) ||
      right_sample != -3 || ::pread(fd, &earlier, sizeof(earlier), 2) != static_cast<ssize_t>(sizeof(earlier)) ||
      earlier != 0) {
    ::close(fd);
    syslab_step("平面布局的声道偏移和写入的样本不一致。");
    return syslab_fail("plane");
  }
  ::close(fd);
  syslab_metric("left_offset", 4);
  syslab_metric("right_offset", 12);
  syslab_note("OS", "planar int16 stores the whole left plane before the right plane");
  syslab_step("帧号 2 的左声道在偏移 4，值是 1000。右声道在偏移 12，值是 -3。帧号 1 的左声道是 0。");
  return syslab_pass();
}
