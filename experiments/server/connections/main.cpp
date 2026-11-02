#include "experiments/common/check.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstring>
#include <mutex>
#include <thread>

// 应用：两条连接上的处理混在同一个执行流里。
// 原理：每个已接受的连接由自己的线程读取。两个 tid 都是正数，并且互不相等。
namespace {

struct Handler {
  int fd = -1;
  long tid = 0;
  char token = 0;
};

std::mutex record_mu;
Handler records[2];

void handle(int fd, int slot) {
  char token = 0;
  if (::read(fd, &token, 1) != 1) {
    token = '?';
  }
  const long tid = ::syscall(SYS_gettid);
  std::lock_guard<std::mutex> guard(record_mu);
  records[slot].fd = fd;
  records[slot].tid = tid;
  records[slot].token = token;
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

  std::thread clients[2];
  const char tokens[2] = {'A', 'B'};
  for (int index = 0; index < 2; ++index) {
    clients[index] = std::thread([address, token = tokens[index]]() {
      const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
      if (fd < 0 || ::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
          ::write(fd, &token, 1) != 1) {
        if (fd >= 0) {
          ::close(fd);
        }
        return;
      }
      ::close(fd);
    });
  }

  std::thread handlers[2];
  int started = 0;
  for (int index = 0; index < 2; ++index) {
    const int connection = ::accept(listener, nullptr, nullptr);
    if (connection < 0) {
      for (int joined = 0; joined < started; ++joined) {
        handlers[joined].join();
      }
      for (std::thread& client : clients) {
        client.join();
      }
      syslab_step("没有接受到两条连接。");
      return syslab_fail("accept");
    }
    handlers[index] = std::thread(handle, connection, index);
    ++started;
  }
  for (std::thread& handler : handlers) {
    handler.join();
  }
  for (std::thread& client : clients) {
    client.join();
  }
  ::close(listener);
  const bool tokens_ok = (records[0].token == 'A' && records[1].token == 'B') ||
                         (records[0].token == 'B' && records[1].token == 'A');
  if (records[0].tid <= 0 || records[1].tid <= 0 || records[0].tid == records[1].tid || !tokens_ok) {
    syslab_step("两条连接没有落在两个不同的 tid 上。");
    return syslab_fail("tid");
  }
  syslab_metric("tid_0", records[0].tid);
  syslab_metric("tid_1", records[1].tid);
  syslab_note("OS", "two accepted connections were read by two tasks with distinct tids");
  syslab_step("两条连接的 tid 都为正数且互不相等，标记 A 和 B 都被读到。");
  return syslab_pass();
}
