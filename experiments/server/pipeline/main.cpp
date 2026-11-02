#include "experiments/common/check.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>

// 应用：一次读入的两帧被合成一次请求，或尾部半个长度被当成第三帧。
// 原理：缓冲按 4 字节大端长度切帧。收齐的帧按到达顺序各算一次请求，不足一帧的尾部留下。
namespace {

constexpr unsigned char kStream[] = {0x00, 0x00, 0x00, 0x04, 'A', 'A', 'A', 'A', 0x00, 0x00, 0x00,
                                    0x04, 'B', 'B', 'B', 'B', 0x00, 0x00};

bool take_frames(const unsigned char* data, std::size_t size, char payloads[][4], int* count, std::size_t* consumed) {
  std::size_t cursor = 0;
  *count = 0;
  while (cursor + 4 <= size && *count < 4) {
    const std::uint32_t length = (std::uint32_t{data[cursor]} << 24) | (std::uint32_t{data[cursor + 1]} << 16) |
                                 (std::uint32_t{data[cursor + 2]} << 8) | std::uint32_t{data[cursor + 3]};
    if (length != 4 || cursor + 8 > size) {
      break;
    }
    std::memcpy(payloads[*count], data + cursor + 4, 4);
    ++(*count);
    cursor += 8;
  }
  *consumed = cursor;
  return true;
}

}  // namespace

int main() {
  int fds[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
    syslab_step("套接字对没有建立。");
    return syslab_fail("socketpair");
  }
  if (::write(fds[1], kStream, sizeof(kStream)) != static_cast<ssize_t>(sizeof(kStream))) {
    syslab_step("帧流没有写完。");
    return syslab_fail("write");
  }
  ::shutdown(fds[1], SHUT_WR);
  unsigned char buffer[64];
  std::size_t filled = 0;
  while (filled < sizeof(buffer)) {
    const ssize_t count = ::read(fds[0], buffer + filled, sizeof(buffer) - filled);
    if (count == 0) {
      break;
    }
    if (count < 0) {
      syslab_step("帧流没有读完。");
      return syslab_fail("read");
    }
    filled += static_cast<std::size_t>(count);
  }
  ::close(fds[0]);
  ::close(fds[1]);
  char payloads[4][4];
  int requests = 0;
  std::size_t consumed = 0;
  take_frames(buffer, filled, payloads, &requests, &consumed);
  if (filled != sizeof(kStream) || requests != 2 || consumed != 16 || filled - consumed != 2 ||
      std::memcmp(payloads[0], "AAAA", 4) != 0 || std::memcmp(payloads[1], "BBBB", 4) != 0 ||
      std::memcmp(buffer + consumed, "\0\0", 2) != 0) {
    syslab_step("两帧和尾部半个长度没有按顺序分开。");
    return syslab_fail("pipeline");
  }
  syslab_metric("requests", requests);
  syslab_note("OS", "one byte stream yields one request per complete frame and keeps a partial trailer");
  syslab_step("一次读入产生两帧，顺序是 AAAA、BBBB。尾部两个字节不构成第三帧。");
  return syslab_pass();
}
