#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

// 现象：O_NOFOLLOW 打开符号链接得到 ELOOP。不带这个标志时读到 DATA。
// 原理：O_NOFOLLOW 要求路径最后一段本身是文件，遇到符号链接就停止并返回 ELOOP。
int main() {
  const int target = ::open("target.bin", O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (target < 0 || ::write(target, "DATA", 4) != 4) {
    if (target >= 0) {
      ::close(target);
    }
    syslab_step("目标文件没有写好。");
    return syslab_fail("write");
  }
  ::close(target);
  if (::symlink("target.bin", "alias") != 0) {
    syslab_step("符号链接没有建立。");
    return syslab_unsupported("symlink");
  }
  const int stopped = ::open("alias", O_RDONLY | O_NOFOLLOW);
  if (stopped >= 0) {
    ::close(stopped);
    syslab_step("O_NOFOLLOW 仍然打开了符号链接。");
    return syslab_fail("followed");
  }
  if (errno != ELOOP) {
    syslab_step("O_NOFOLLOW 的错误不是 ELOOP。");
    return syslab_fail("errno");
  }
  const int opened = ::open("alias", O_RDONLY);
  char body[4] = {};
  if (opened < 0 || ::read(opened, body, 4) != 4 || std::memcmp(body, "DATA", 4) != 0) {
    if (opened >= 0) {
      ::close(opened);
    }
    syslab_step("不带 O_NOFOLLOW 时没有读到 DATA。");
    return syslab_fail("follow");
  }
  ::close(opened);
  syslab_note("OS", "O_NOFOLLOW rejects a final symlink with ELOOP; open without it reads the target");
  syslab_step("O_NOFOLLOW 打开 alias 返回 ELOOP。不带该标志时读到 DATA。");
  return syslab_pass();
}
