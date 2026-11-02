#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>

// 应用：页上有一个字节和写入时不一致，读路径仍把它当成有效记录。
// 原理：校验和覆盖页内 8 个字节。校验和不符时不返回载荷。
namespace {

constexpr char kPayload[] = {'R', 'E', 'C', 'O', 'R', 'D', '0', '1'};

std::uint32_t checksum(const unsigned char data[8]) {
  std::uint32_t value = 0;
  for (int index = 0; index < 8; ++index) {
    value = value * 131u + data[index];
  }
  return value;
}

bool write_page(int fd, const unsigned char data[8], std::uint32_t sum) {
  unsigned char bytes[12];
  std::memcpy(bytes, data, 8);
  std::memcpy(bytes + 8, &sum, sizeof(sum));
  return ::lseek(fd, 0, SEEK_SET) == 0 && ::write(fd, bytes, sizeof(bytes)) == static_cast<ssize_t>(sizeof(bytes)) &&
         ::fsync(fd) == 0;
}

bool read_page(int fd, unsigned char data[8], std::uint32_t* sum) {
  unsigned char bytes[12];
  if (::lseek(fd, 0, SEEK_SET) != 0 || ::read(fd, bytes, sizeof(bytes)) != static_cast<ssize_t>(sizeof(bytes))) {
    return false;
  }
  std::memcpy(data, bytes, 8);
  std::memcpy(sum, bytes + 8, sizeof(*sum));
  return true;
}

bool accept_page(const unsigned char data[8], std::uint32_t sum, unsigned char out[8]) {
  if (checksum(data) != sum) {
    return false;
  }
  std::memcpy(out, data, 8);
  return true;
}

}  // namespace

int main() {
  const int fd = ::open("page.bin", O_CREAT | O_TRUNC | O_RDWR, 0644);
  if (fd < 0) {
    syslab_step("页文件没有打开。");
    return syslab_fail("open");
  }
  unsigned char stored[8];
  std::memcpy(stored, kPayload, 8);
  if (!write_page(fd, stored, checksum(stored))) {
    syslab_step("有效页没有写稳。");
    return syslab_unsupported("fsync");
  }
  unsigned char loaded[8];
  std::uint32_t sum = 0;
  unsigned char accepted[8];
  std::memset(accepted, 0x5A, sizeof(accepted));
  if (!read_page(fd, loaded, &sum) || !accept_page(loaded, sum, accepted) || std::memcmp(accepted, kPayload, 8) != 0) {
    syslab_step("校验和相符的页没有返回载荷。");
    return syslab_fail("valid");
  }
  loaded[0] = static_cast<unsigned char>(loaded[0] ^ 0xFF);
  if (!write_page(fd, loaded, sum)) {
    syslab_step("撕页没有写稳。");
    return syslab_unsupported("fsync");
  }
  std::memset(accepted, 0x5A, sizeof(accepted));
  if (!read_page(fd, loaded, &sum) || accept_page(loaded, sum, accepted) || accepted[0] != 0x5A) {
    syslab_step("校验和不符的页返回了载荷。");
    return syslab_fail("torn");
  }
  ::close(fd);
  syslab_note("OS", "a page checksum rejects a changed byte until the checksum is rewritten");
  syslab_step("校验和相符时返回 RECORD01。改动一个字节且不更新校验和时，不返回载荷。");
  return syslab_pass();
}
