#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>

// 现象：帧号 2 的左声道写成 1000 后，右声道仍是 -3，帧号 1 的两个声道仍是 0。帧号从 0 计。
// 原理：交错存放的 int16 立体声里，第 i 帧左声道在偏移 i * 4，右声道紧随其后 2 字节。
int main() {
  std::int16_t frames[4][2] = {};
  frames[2][0] = 1000;
  frames[2][1] = -3;
  const int fd = ::open("stereo.pcm", O_CREAT | O_TRUNC | O_RDWR, 0644);
  if (fd < 0 || ::write(fd, frames, sizeof(frames)) != static_cast<ssize_t>(sizeof(frames))) {
    if (fd >= 0) {
      ::close(fd);
    }
    syslab_step("采样没有写完。");
    return syslab_fail("write");
  }
  std::int16_t pair[2] = {};
  std::int16_t earlier[2] = {1, 1};
  const off_t frame_two = 2 * 4;
  if (::pread(fd, pair, sizeof(pair), frame_two) != static_cast<ssize_t>(sizeof(pair)) || pair[0] != 1000 ||
      pair[1] != -3 || ::pread(fd, earlier, sizeof(earlier), 4) != static_cast<ssize_t>(sizeof(earlier)) ||
      earlier[0] != 0 || earlier[1] != 0) {
    ::close(fd);
    syslab_step("声道偏移上的样本和写入的不一致。");
    return syslab_fail("channel");
  }
  ::close(fd);
  syslab_metric("frame", 2);
  syslab_metric("left", pair[0]);
  syslab_note("OS", "interleaved int16 stereo places frame i at byte offset i * 4");
  syslab_step("帧号 2 的左声道是 1000，右声道是 -3。帧号 1 的两个声道都是 0。");
  return syslab_pass();
}
