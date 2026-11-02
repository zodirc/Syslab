#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "experiments/common/check.hpp"
#include "experiments/common/userns.hpp"

#include <fcntl.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>

// 应用：容器里挂上的文件系统在容器外也能看见。
// 原理：新的挂载命名空间先变成私有，再把 tmpfs 挂到目录上。命名空间外的进程看不到其中的标记文件。
int main() {
  if (::mkdir("vis", 0755) != 0) {
    syslab_step("挂载点目录没有建立。");
    return syslab_fail("mkdir");
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
    _exit(::access("vis/marker", F_OK) == 0 ? 2 : 0);
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
  if (::unshare(CLONE_NEWNS) != 0 || ::mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
    syslab_step("挂载命名空间没有变成私有。");
    return finish(syslab_unsupported("mount"));
  }
  if (::mount("tmpfs", "vis", "tmpfs", 0, nullptr) != 0) {
    syslab_step("tmpfs 没有挂上。");
    return finish(syslab_fail("tmpfs"));
  }
  const int marker = ::open("vis/marker", O_CREAT | O_RDWR | O_TRUNC, 0644);
  if (marker < 0 || ::write(marker, "in", 2) != 2) {
    syslab_step("标记文件没有写入挂载点。");
    return finish(syslab_fail("marker"));
  }
  char back[2] = {};
  if (::lseek(marker, 0, SEEK_SET) != 0 || ::read(marker, back, 2) != 2 || std::memcmp(back, "in", 2) != 0) {
    syslab_step("挂载点内没有读回标记。");
    return finish(syslab_fail("read"));
  }
  ::close(marker);
  const char done = 1;
  if (::write(pipe_fd[1], &done, 1) != 1) {
    return finish(syslab_fail("signal"));
  }
  ::close(pipe_fd[1]);
  int child_status = 0;
  if (::waitpid(sibling, &child_status, 0) < 0 || !WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0) {
    syslab_step("挂载命名空间外的进程看见了标记文件。");
    return syslab_fail("visible");
  }
  syslab_note("OS", "MS_PRIVATE keeps the tmpfs marker inside the new mount namespace");
  syslab_step("新挂载命名空间里的 vis/marker 读回 in。进入命名空间之前的进程看不到该文件。");
  return syslab_pass();
}
