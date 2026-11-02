#include "experiments/common/check.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <thread>

// 应用：队列已满时，新的工作项仍被收进完成集合。
// 原理：容量为 2。前两个连接的标记入队并得到 OKAY。第三个连接得到 BUSY，不进入队列。
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

void client(sockaddr_in address, char token, char reply[4], int* ok) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0 || ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
      ::write(fd, &token, 1) != 1 || ::read(fd, reply, 4) != 4) {
    *ok = 0;
  } else {
    *ok = 1;
  }
  if (fd >= 0) {
    ::close(fd);
  }
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
  if (::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || ::listen(listener, 3) != 0) {
    syslab_step("没有在回环地址上开始监听。");
    return syslab_fail("bind");
  }
  socklen_t length = sizeof(address);
  if (::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
    syslab_step("没有读到监听端口。");
    return syslab_fail("getsockname");
  }

  const char tokens[3] = {'A', 'B', 'C'};
  char replies[3][4] = {};
  int client_ok[3] = {};
  std::thread clients[3];
  for (int index = 0; index < 3; ++index) {
    clients[index] = std::thread(client, address, tokens[index], replies[index], &client_ok[index]);
  }

  char queued[2] = {};
  int admitted = 0;
  int busy = 0;
  for (int index = 0; index < 3; ++index) {
    const int connection = ::accept(listener, nullptr, nullptr);
    if (connection < 0) {
      for (std::thread& worker : clients) {
        worker.join();
      }
      syslab_step("没有接受到三条连接。");
      return syslab_fail("accept");
    }
    char token = 0;
    if (::read(connection, &token, 1) != 1) {
      ::close(connection);
      for (std::thread& worker : clients) {
        worker.join();
      }
      syslab_step("工作项没有读到。");
      return syslab_fail("read");
    }
    if (admitted < 2) {
      queued[admitted] = token;
      ++admitted;
      write_full(connection, "OKAY", 4);
    } else {
      ++busy;
      write_full(connection, "BUSY", 4);
    }
    ::close(connection);
  }
  for (std::thread& worker : clients) {
    worker.join();
  }
  ::close(listener);

  int okay_replies = 0;
  int busy_replies = 0;
  bool seen[3] = {};
  for (int index = 0; index < 3; ++index) {
    if (client_ok[index] != 1) {
      syslab_step("有连接没有得到完整回答。");
      return syslab_fail("client");
    }
    if (std::memcmp(replies[index], "OKAY", 4) == 0) {
      ++okay_replies;
    } else if (std::memcmp(replies[index], "BUSY", 4) == 0) {
      ++busy_replies;
    }
  }
  if (admitted != 2 || busy != 1 || okay_replies != 2 || busy_replies != 1 || queued[0] == queued[1]) {
    syslab_step("容量为 2 的队列没有把第三个工作项拒在外面。");
    return syslab_fail("capacity");
  }
  for (int index = 0; index < 2; ++index) {
    const char token = queued[index];
    if (token < 'A' || token > 'C' || seen[token - 'A']) {
      syslab_step("入队的标记重复或不在 A、B、C 中。");
      return syslab_fail("queue");
    }
    seen[token - 'A'] = true;
  }
  syslab_metric("admitted", admitted);
  syslab_metric("busy", busy);
  syslab_note("OS", "a full queue refuses the next work item instead of appending it");
  syslab_step("两个不同标记入队并得到 OKAY。第三个连接得到 BUSY，没有进入队列。");
  return syslab_pass();
}
