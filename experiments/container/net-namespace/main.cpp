#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "experiments/common/check.hpp"
#include "experiments/common/userns.hpp"

#include <linux/if.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

// 应用：容器里仍能按宿主机的接口名找到网卡。
// 原理：新的网络命名空间有 lo。进入命名空间之前存在的另一块网卡，在新命名空间里没有接口索引。
namespace {

int interface_index(const char* name) {
  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return -1;
  }
  struct ifreq request{};
  std::snprintf(request.ifr_name, sizeof(request.ifr_name), "%s", name);
  const int result = ::ioctl(fd, SIOCGIFINDEX, &request);
  ::close(fd);
  if (result != 0) {
    return -1;
  }
  return request.ifr_ifindex;
}

bool find_foreign(char name[IFNAMSIZ]) {
  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return false;
  }
  struct ifreq buffer[32];
  struct ifconf config{};
  config.ifc_len = sizeof(buffer);
  config.ifc_req = buffer;
  if (::ioctl(fd, SIOCGIFCONF, &config) != 0) {
    ::close(fd);
    return false;
  }
  ::close(fd);
  const int count = config.ifc_len / static_cast<int>(sizeof(struct ifreq));
  for (int index = 0; index < count; ++index) {
    if (std::strcmp(buffer[index].ifr_name, "lo") != 0 && buffer[index].ifr_name[0] != '\0') {
      std::snprintf(name, IFNAMSIZ, "%s", buffer[index].ifr_name);
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  char foreign[IFNAMSIZ] = {};
  if (!find_foreign(foreign) || interface_index("lo") <= 0) {
    syslab_step("进入网络命名空间之前没有找到 lo 以外的已配置接口。");
    return syslab_unsupported("iface");
  }
  int pipe_fd[2] = {-1, -1};
  if (::pipe(pipe_fd) != 0) {
    syslab_step("管道没有建立。");
    return syslab_fail("pipe");
  }
  const pid_t sibling = ::fork();
  if (sibling < 0) {
    syslab_step("fork 失败。");
    return syslab_fail("fork");
  }
  if (sibling == 0) {
    ::close(pipe_fd[1]);
    char ready = 0;
    if (::read(pipe_fd[0], &ready, 1) != 1) {
      _exit(3);
    }
    _exit(interface_index(foreign) > 0 ? 0 : 2);
  }
  ::close(pipe_fd[0]);

  auto finish = [&](int status) {
    const char done = 1;
    ::write(pipe_fd[1], &done, 1);
    ::close(pipe_fd[1]);
    int child_status = 0;
    ::waitpid(sibling, &child_status, 0);
    return status;
  };

  if (syslab_enter_user_namespace() != 0) {
    syslab_step("用户命名空间没有建立。");
    return finish(syslab_unsupported("userns"));
  }
  if (::unshare(CLONE_NEWNET) != 0) {
    syslab_step("网络命名空间没有建立。");
    return finish(syslab_unsupported("net"));
  }
  if (interface_index("lo") <= 0 || interface_index(foreign) > 0) {
    syslab_step("新网络命名空间仍能索引进入之前的那块网卡。");
    return finish(syslab_fail("index"));
  }
  const char done = 1;
  if (::write(pipe_fd[1], &done, 1) != 1) {
    return finish(syslab_fail("signal"));
  }
  ::close(pipe_fd[1]);
  int child_status = 0;
  if (::waitpid(sibling, &child_status, 0) < 0 || !WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0) {
    syslab_step("原网络命名空间里的接口索引消失了。");
    return syslab_fail("sibling");
  }
  syslab_metric("loopback_index", interface_index("lo"));
  syslab_note("OS", "SIOCGIFINDEX in a new net namespace sees lo and not the sibling namespace interface");
  syslab_step("新网络命名空间可以索引 lo，不能索引进入之前的那块网卡。原命名空间仍能索引那块网卡。");
  return syslab_pass();
}
