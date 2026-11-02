#include "experiments/common/check.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>

// 应用：半个长度前缀或半段载荷被当成了一次请求。
// 原理：请求是 4 字节大端长度加上等长的载荷。两边没有收齐时，请求数保持为 0。
namespace {

std::atomic<int> requests{0};
char accepted[8] = {};

void serve(int fd) {
  unsigned char pending[64];
  std::size_t filled = 0;
  while (requests.load() < 1) {
    const ssize_t count = ::read(fd, pending + filled, sizeof(pending) - filled);
    if (count <= 0) {
      return;
    }
    filled += static_cast<std::size_t>(count);
    while (filled >= 4 && requests.load() < 1) {
      const std::uint32_t length = (std::uint32_t{pending[0]} << 24) | (std::uint32_t{pending[1]} << 16) |
                                   (std::uint32_t{pending[2]} << 8) | std::uint32_t{pending[3]};
      if (length > 8 || filled < 4u + length) {
        break;
      }
      std::memcpy(accepted, pending + 4, length);
      std::memmove(pending, pending + 4 + length, filled - (4u + length));
      filled -= 4u + length;
      requests.store(1);
    }
  }
}

}  // namespace

int main() {
  int fds[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
    syslab_step("套接字对没有建立。");
    return syslab_fail("socketpair");
  }
  std::thread reader(serve, fds[0]);
  const unsigned char partial_header[] = {0x00, 0x00};
  if (::write(fds[1], partial_header, 2) != 2) {
    reader.join();
    return syslab_fail("write");
  }
  ::usleep(150000);
  if (requests.load() != 0) {
    reader.join();
    syslab_step("长度只到达两个字节时已经产生了请求。");
    return syslab_fail("header");
  }
  const unsigned char rest_header_and_one[] = {0x00, 0x04, 'p'};
  if (::write(fds[1], rest_header_and_one, 3) != 3) {
    reader.join();
    return syslab_fail("write");
  }
  ::usleep(150000);
  if (requests.load() != 0) {
    reader.join();
    syslab_step("载荷尚未收齐时已经产生了请求。");
    return syslab_fail("body");
  }
  if (::write(fds[1], "ing", 3) != 3) {
    reader.join();
    return syslab_fail("write");
  }
  reader.join();
  ::close(fds[0]);
  ::close(fds[1]);
  if (requests.load() != 1 || std::memcmp(accepted, "ping", 4) != 0) {
    syslab_step("收齐之后没有得到唯一的一次 ping 请求。");
    return syslab_fail("frame");
  }
  syslab_metric("requests", requests.load());
  syslab_note("OS", "a frame is one request only after its 4-byte length and payload have both arrived");
  syslab_step("长度或载荷未收齐时请求数是 0。收齐后请求数是 1，载荷是 ping。");
  return syslab_pass();
}
