#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>

// 应用：恢复时用日志里的旧记录覆盖了检查点已经保存的页。
// 原理：先安装检查点页镜像。重放只应用 lsn 大于检查点的记录。
namespace {

struct Record {
  std::uint64_t lsn;
  std::uint32_t offset;
  char data[4];
};

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

bool read_exact(int fd, void* data, std::size_t size) {
  char* cursor = static_cast<char*>(data);
  std::size_t got = 0;
  while (got < size) {
    const ssize_t count = ::read(fd, cursor + got, size - got);
    if (count <= 0) {
      return false;
    }
    got += static_cast<std::size_t>(count);
  }
  return true;
}

bool store_bytes(int fd, const void* data, std::size_t size) {
  return ::lseek(fd, 0, SEEK_SET) == 0 && write_full(fd, data, size) && ::fsync(fd) == 0;
}

}  // namespace

int main() {
  const int page_fd = ::open("page.bin", O_CREAT | O_TRUNC | O_RDWR, 0644);
  const int checkpoint_fd = ::open("checkpoint.bin", O_CREAT | O_TRUNC | O_RDWR, 0644);
  const int wal_fd = ::open("wal.bin", O_CREAT | O_TRUNC | O_RDWR, 0644);
  if (page_fd < 0 || checkpoint_fd < 0 || wal_fd < 0) {
    syslab_step("页、检查点或日志没有打开。");
    return syslab_fail("open");
  }
  unsigned char stale[16];
  std::memset(stale, '.', 8);
  std::uint64_t stale_lsn = 0;
  std::memcpy(stale + 8, &stale_lsn, sizeof(stale_lsn));
  unsigned char image[16];
  std::memcpy(image, "KEEP....", 8);
  std::uint64_t checkpoint_lsn = 1;
  std::memcpy(image + 8, &checkpoint_lsn, sizeof(checkpoint_lsn));
  Record records[2]{};
  records[0].lsn = 1;
  records[0].offset = 0;
  std::memcpy(records[0].data, "LOST", 4);
  records[1].lsn = 2;
  records[1].offset = 4;
  std::memcpy(records[1].data, "TAIL", 4);
  if (!store_bytes(page_fd, stale, sizeof(stale)) || !store_bytes(checkpoint_fd, image, sizeof(image)) ||
      !store_bytes(wal_fd, records, sizeof(records))) {
    syslab_step("检查点或日志没有写稳。");
    return syslab_unsupported("fsync");
  }

  unsigned char page[16];
  unsigned char checkpoint[16];
  if (::lseek(page_fd, 0, SEEK_SET) != 0 || !read_exact(page_fd, page, sizeof(page)) ||
      ::lseek(checkpoint_fd, 0, SEEK_SET) != 0 || !read_exact(checkpoint_fd, checkpoint, sizeof(checkpoint))) {
    syslab_step("页或检查点没有读回。");
    return syslab_fail("read");
  }
  std::memcpy(page, checkpoint, 8);
  std::uint64_t applied = 0;
  std::memcpy(&applied, checkpoint + 8, sizeof(applied));
  if (::lseek(wal_fd, 0, SEEK_SET) != 0) {
    syslab_step("日志没有回到开头。");
    return syslab_fail("seek");
  }
  for (;;) {
    Record record{};
    const ssize_t count = ::read(wal_fd, &record, sizeof(record));
    if (count == 0) {
      break;
    }
    if (count != static_cast<ssize_t>(sizeof(record)) || record.offset > 4) {
      syslab_step("日志记录不完整。");
      return syslab_fail("record");
    }
    if (record.lsn <= applied) {
      continue;
    }
    std::memcpy(page + record.offset, record.data, 4);
    applied = record.lsn;
  }
  std::memcpy(page + 8, &applied, sizeof(applied));
  if (!store_bytes(page_fd, page, sizeof(page))) {
    syslab_step("恢复后的页没有写稳。");
    return syslab_unsupported("fsync");
  }
  unsigned char recovered[16];
  if (::lseek(page_fd, 0, SEEK_SET) != 0 || !read_exact(page_fd, recovered, sizeof(recovered)) ||
      std::memcmp(recovered, "KEEPTAIL", 8) != 0 || applied != 2) {
    syslab_step("恢复结果不是检查点镜像加上检查点之后的记录。");
    return syslab_fail("recover");
  }
  ::close(page_fd);
  ::close(checkpoint_fd);
  ::close(wal_fd);
  syslab_metric("applied_lsn", static_cast<long long>(applied));
  syslab_note("OS", "recovery installs the checkpoint image, then applies WAL records with a greater lsn");
  syslab_step("检查点镜像 KEEP 保留。lsn 2 写入后，页是 KEEPTAIL，applied_lsn 是 2。");
  return syslab_pass();
}
