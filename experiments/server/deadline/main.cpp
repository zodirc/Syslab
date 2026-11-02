#include "experiments/common/check.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <thread>

// 应用：一个不继续发送的连接把已经收齐的连接一起拖住。
// 原理：poll 的期限内没有收齐长度和载荷时关闭该连接。同一轮里收齐的连接得到 OK。
namespace {

bool write_full(int fd, const void* data, std::size_t size) {
  const char* cursor = static_cast<const char*>(data);
  std::size_t sent = 0;
  while (sent < size) {
    const ssize_t count = ::write(fd, cursor + sent, size - sent);
    if (count < 0) {
      return false;
    }
    sent += static_cast<std::size_t>(count);
  }
  return true;
}

bool read_frame(int fd, char* payload) {
  unsigned char pending[16];
  std::size_t filled = 0;
  while (filled < 4) {
    pollfd wait_read{};
    wait_read.fd = fd;
    wait_read.events = POLLIN;
    if (::poll(&wait_read, 1, 400) <= 0) {
      return false;
    }
    const ssize_t count = ::read(fd, pending + filled, sizeof(pending) - filled);
    if (count <= 0) {
      return false;
    }
    filled += static_cast<std::size_t>(count);
  }
  const std::uint32_t length = (std::uint32_t{pending[0]} << 24) | (std::uint32_t{pending[1]} << 16) |
                               (std::uint32_t{pending[2]} << 8) | std::uint32_t{pending[3]};
  if (length != 4) {
    return false;
  }
  while (filled < 8) {
    pollfd wait_read{};
    wait_read.fd = fd;
    wait_read.events = POLLIN;
    if (::poll(&wait_read, 1, 400) <= 0) {
      return false;
    }
    const ssize_t count = ::read(fd, pending + filled, 8 - filled);
    if (count <= 0) {
      return false;
    }
    filled += static_cast<std::size_t>(count);
  }
  std::memcpy(payload, pending + 4, 4);
  return true;
}

}  // namespace

int main() {
  const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listener < 0) {
    syslab_step("监听套接字没有建立。");
    return syslab_fail("socket");
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || ::listen(listener, 2) != 0) {
    syslab_step("没有在回环地址上开始监听。");
    return syslab_fail("bind");
  }
  socklen_t length = sizeof(address);
  if (::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
    syslab_step("没有读到监听端口。");
    return syslab_fail("getsockname");
  }

  int fast_ok = 0;
  std::thread slow([address]() {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0 || ::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
      if (fd >= 0) {
        ::close(fd);
      }
      return;
    }
    char ignored = 0;
    ::read(fd, &ignored, 1);
    ::close(fd);
  });
  std::thread fast([address, &fast_ok]() {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    const unsigned char frame[] = {0x00, 0x00, 0x00, 0x04, 'F', 'A', 'S', 'T'};
    char reply[2] = {};
    if (fd >= 0 && ::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0 &&
        write_full(fd, frame, sizeof(frame)) && ::read(fd, reply, 2) == 2 && std::memcmp(reply, "OK", 2) == 0) {
      fast_ok = 1;
    }
    if (fd >= 0) {
      ::close(fd);
    }
  });

  int timeouts = 0;
  int replies = 0;
  for (int index = 0; index < 2; ++index) {
    const int connection = ::accept(listener, nullptr, nullptr);
    if (connection < 0) {
      slow.join();
      fast.join();
      syslab_step("没有接受到两条连接。");
      return syslab_fail("accept");
    }
    char payload[4] = {};
    if (!read_frame(connection, payload)) {
      ++timeouts;
      ::close(connection);
      continue;
    }
    if (std::memcmp(payload, "FAST", 4) == 0 && write_full(connection, "OK", 2)) {
      ++replies;
    }
    ::close(connection);
  }
  slow.join();
  fast.join();
  ::close(listener);
  if (timeouts != 1 || replies != 1 || fast_ok != 1) {
    syslab_step("期限内未收齐的连接和已收齐的连接没有被分开。");
    return syslab_fail("deadline");
  }
  syslab_metric("timeouts", timeouts);
  syslab_metric("replies", replies);
  syslab_note("OS", "a connection that does not complete a frame within the poll deadline is closed");
  syslab_step("一条连接在 400 毫秒内没有收齐帧并被关闭。另一条连接收到 OK。");
  return syslab_pass();
}
