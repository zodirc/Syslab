#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "experiments/common/check.hpp"
#include "experiments/common/userns.hpp"

#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>

// 应用：容器里修改主机名之后，容器外的主机名也变了。
// 原理：UTS 命名空间有自己的主机名。命名空间外的进程仍读到原来的主机名。
int main() {
  char original[64] = {};
  if (::gethostname(original, sizeof(original) - 1) != 0 || original[0] == '\0') {
    syslab_step("没有读到当前主机名。");
    return syslab_fail("gethostname");
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
    char current[64] = {};
    if (::gethostname(current, sizeof(current) - 1) != 0) {
      _exit(4);
    }
    _exit(std::strcmp(current, original) == 0 ? 0 : 2);
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
  if (::unshare(CLONE_NEWUTS) != 0 || ::sethostname("syslab-ns", 9) != 0) {
    syslab_step("UTS 命名空间里的主机名没有改成 syslab-ns。");
    return finish(syslab_unsupported("uts"));
  }
  char changed[64] = {};
  if (::gethostname(changed, sizeof(changed) - 1) != 0 || std::strcmp(changed, "syslab-ns") != 0) {
    syslab_step("新 UTS 命名空间没有读到 syslab-ns。");
    return finish(syslab_fail("hostname"));
  }
  const char done = 1;
  if (::write(pipe_fd[1], &done, 1) != 1) {
    return finish(syslab_fail("signal"));
  }
  ::close(pipe_fd[1]);
  int child_status = 0;
  if (::waitpid(sibling, &child_status, 0) < 0 || !WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0) {
    syslab_step("UTS 命名空间外的主机名发生了变化。");
    return syslab_fail("visible");
  }
  syslab_note("OS", "sethostname in a new UTS namespace leaves the sibling hostname unchanged");
  syslab_step("新 UTS 命名空间的主机名是 syslab-ns。命名空间外的进程仍读到原来的主机名。");
  return syslab_pass();
}
