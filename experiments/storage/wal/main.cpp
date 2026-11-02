#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>

// 应用：重放多出或少了一段修改时，先核对日志和页上已经应用的序号。
// 原理：日志记录先追加并 fsync。页上的 applied_lsn 较小的记录会在重放时写入。截断掉的记录不再写入。
namespace {

struct Page {
  std::uint64_t applied_lsn;
  char data[8];
};

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

bool read_full(int fd, void* data, std::size_t size) {
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

bool store_page(int fd, const Page& page) {
  return ::lseek(fd, 0, SEEK_SET) == 0 && write_full(fd, &page, sizeof(page)) && ::fsync(fd) == 0;
}

bool load_page(int fd, Page* page) {
  return ::lseek(fd, 0, SEEK_SET) == 0 && read_full(fd, page, sizeof(*page));
}

bool append_record(int fd, const Record& record) {
  return write_full(fd, &record, sizeof(record)) && ::fsync(fd) == 0;
}

bool replay(int page_fd, int wal_fd) {
  Page page{};
  if (!load_page(page_fd, &page) || ::lseek(wal_fd, 0, SEEK_SET) != 0) {
    return false;
  }
  for (;;) {
    Record record{};
    const ssize_t count = ::read(wal_fd, &record, sizeof(record));
    if (count == 0) {
      break;
    }
    if (count != static_cast<ssize_t>(sizeof(record))) {
      return false;
    }
    if (record.lsn <= page.applied_lsn) {
      continue;
    }
    if (record.offset > 4) {
      return false;
    }
    std::memcpy(page.data + record.offset, record.data, 4);
    page.applied_lsn = record.lsn;
  }
  return store_page(page_fd, page);
}

}  // namespace

int main() {
  const int page_fd = ::open("page.bin", O_CREAT | O_TRUNC | O_RDWR, 0644);
  const int wal_fd = ::open("wal.bin", O_CREAT | O_TRUNC | O_RDWR, 0644);
  if (page_fd < 0 || wal_fd < 0) {
    syslab_step("页或日志没有打开。");
    return syslab_fail("open");
  }
  Page committed{};
  committed.applied_lsn = 0;
  std::memset(committed.data, '.', 8);
  if (!store_page(page_fd, committed)) {
    syslab_step("页的初始镜像没有写稳。");
    return syslab_unsupported("fsync");
  }
  Record first{};
  first.lsn = 1;
  first.offset = 0;
  std::memcpy(first.data, "ABCD", 4);
  if (!append_record(wal_fd, first)) {
    syslab_step("第一条日志没有写稳。");
    return syslab_unsupported("fsync");
  }
  std::memcpy(committed.data, "ABCD", 4);
  committed.applied_lsn = 1;
  if (!store_page(page_fd, committed)) {
    syslab_step("已提交的页没有写稳。");
    return syslab_unsupported("fsync");
  }
  const off_t committed_wal = ::lseek(wal_fd, 0, SEEK_CUR);
  Record second{};
  second.lsn = 2;
  second.offset = 4;
  std::memcpy(second.data, "EFGH", 4);
  if (!append_record(wal_fd, second)) {
    syslab_step("第二条日志没有写稳。");
    return syslab_unsupported("fsync");
  }
  if (!replay(page_fd, wal_fd)) {
    syslab_step("完整日志没有重放完。");
    return syslab_fail("replay");
  }
  Page restored{};
  if (!load_page(page_fd, &restored) || restored.applied_lsn != 2 ||
      std::memcmp(restored.data, "ABCDEFGH", 8) != 0) {
    syslab_step("完整日志重放后的页不是 ABCDEFGH。");
    return syslab_fail("full");
  }
  if (!store_page(page_fd, committed) || ::ftruncate(wal_fd, committed_wal) != 0 || ::fsync(wal_fd) != 0) {
    syslab_step("日志没有截断到已提交记录。");
    return syslab_unsupported("fsync");
  }
  if (!replay(page_fd, wal_fd)) {
    syslab_step("截断后的日志没有重放完。");
    return syslab_fail("truncated-replay");
  }
  Page truncated{};
  if (!load_page(page_fd, &truncated) || truncated.applied_lsn != 1 ||
      std::memcmp(truncated.data, "ABCD....", 8) != 0) {
    syslab_step("截断日志之后的重放仍包含第二条记录。");
    return syslab_fail("truncate");
  }
  ::close(page_fd);
  ::close(wal_fd);
  syslab_metric("applied_lsn", static_cast<long long>(truncated.applied_lsn));
  syslab_note("OS", "replay applies WAL records whose lsn is greater than the page applied_lsn");
  syslab_step("完整日志重放得到 ABCDEFGH。截断到第一条已提交记录后再重放，页保持 ABCD....。");
  return syslab_pass();
}
