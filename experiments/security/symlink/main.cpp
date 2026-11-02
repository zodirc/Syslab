#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

// 现象：打开符号链接读到目标文件的 DATA。readlink 读到的是路径 target.bin。
// 原理：open 解析符号链接并打开目标。readlink 不跟随，返回链接里保存的路径。
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
  const int opened = ::open("alias", O_RDONLY);
  char body[4] = {};
  if (opened < 0 || ::read(opened, body, 4) != 4 || std::memcmp(body, "DATA", 4) != 0) {
    if (opened >= 0) {
      ::close(opened);
    }
    syslab_step("打开符号链接没有读到目标文件的 DATA。");
    return syslab_fail("follow");
  }
  ::close(opened);
  char link_text[64] = {};
  const ssize_t length = ::readlink("alias", link_text, sizeof(link_text));
  if (length != 10 || std::memcmp(link_text, "target.bin", 10) != 0) {
    syslab_step("readlink 没有返回 target.bin。");
    return syslab_fail("readlink");
  }
  syslab_note("OS", "open follows a symlink to the target bytes; readlink returns the stored path");
  syslab_step("open alias 读到 DATA。readlink 返回 target.bin。");
  return syslab_pass();
}
