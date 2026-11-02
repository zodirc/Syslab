#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "experiments/common/check.hpp"
#include "experiments/common/userns.hpp"

#include <fcntl.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

// 应用：进程打开了准备目录以外的文件。
// 原理：私有挂载命名空间里 chroot 到准备好的目录之后，打开的路径落在该目录里。
int main() {
  if (syslab_enter_user_namespace() != 0) {
    syslab_step("用户命名空间没有建立。");
    return syslab_unsupported("userns");
  }
  if (::unshare(CLONE_NEWNS) != 0 || ::mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
    syslab_step("挂载命名空间没有变成私有。");
    return syslab_unsupported("mount");
  }
  if (::mkdir("jail", 0755) != 0 || ::mkdir("jail/data", 0755) != 0) {
    syslab_step("chroot 目录没有建立。");
    return syslab_fail("mkdir");
  }
  const int outside = ::open("outside", O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (outside < 0 || ::write(outside, "out", 3) != 3) {
    syslab_step("目录外的文件没有写好。");
    return syslab_fail("outside");
  }
  ::close(outside);
  const int token = ::open("jail/data/token", O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (token < 0 || ::write(token, "inside", 6) != 6) {
    syslab_step("目录内的 token 没有写好。");
    return syslab_fail("token");
  }
  ::close(token);
  if (::chdir("jail") != 0 || ::chroot(".") != 0 || ::chdir("/") != 0) {
    syslab_step("chroot 没有完成。");
    return syslab_unsupported("chroot");
  }
  const int inside = ::open("/data/token", O_RDONLY);
  char body[6] = {};
  if (inside < 0 || ::read(inside, body, 6) != 6 || std::memcmp(body, "inside", 6) != 0) {
    syslab_step("chroot 之后没有读到目录内的 token。");
    return syslab_fail("inside");
  }
  ::close(inside);
  const int escaped = ::open("/outside", O_RDONLY);
  if (escaped >= 0) {
    ::close(escaped);
    syslab_step("chroot 之后仍然打开了目录外的 outside。");
    return syslab_fail("escaped");
  }
  syslab_note("OS", "after chroot, open resolves paths inside the prepared directory");
  syslab_step("chroot 之后 /data/token 读到 inside。/outside 打不开。");
  return syslab_pass();
}
