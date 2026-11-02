#include "experiments/common/check.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>

// 应用：查找命中了不该命中的键，或漏掉了已经写入的键。
// 原理：索引保存键和载荷偏移。没有占用的键不能返回某个载荷。
namespace {

constexpr int kSlots = 4;

struct Entry {
  char key[8];
  std::uint32_t offset;
  std::uint32_t length;
  std::uint32_t used;
};

constexpr std::uint32_t kIndexBytes = kSlots * sizeof(Entry);

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

bool read_at(int fd, void* data, std::size_t size, off_t offset) {
  char* cursor = static_cast<char*>(data);
  std::size_t got = 0;
  while (got < size) {
    const ssize_t count = ::pread(fd, cursor + got, size - got, offset + static_cast<off_t>(got));
    if (count <= 0) {
      return false;
    }
    got += static_cast<std::size_t>(count);
  }
  return true;
}

void set_key(char key[8], const char* text) {
  std::memset(key, 0, 8);
  std::memcpy(key, text, std::strlen(text));
}

bool lookup(int fd, const char key[8], char* payload, std::uint32_t* length) {
  for (int slot = 0; slot < kSlots; ++slot) {
    Entry entry{};
    if (!read_at(fd, &entry, sizeof(entry), static_cast<off_t>(slot * sizeof(Entry)))) {
      return false;
    }
    if (entry.used != 1 || std::memcmp(entry.key, key, 8) != 0) {
      continue;
    }
    if (entry.length > 8 || !read_at(fd, payload, entry.length, static_cast<off_t>(entry.offset))) {
      return false;
    }
    *length = entry.length;
    return true;
  }
  return false;
}

}  // namespace

int main() {
  const int fd = ::open("index.bin", O_CREAT | O_TRUNC | O_RDWR, 0644);
  if (fd < 0) {
    syslab_step("索引文件没有打开。");
    return syslab_fail("open");
  }
  Entry entries[kSlots]{};
  const char* keys[] = {"alpha", "beta", "gamma"};
  const char* payloads[] = {"ALPHA", "BETA", "GAMMA"};
  std::uint32_t data_offset = kIndexBytes;
  for (int index = 0; index < 3; ++index) {
    set_key(entries[index].key, keys[index]);
    entries[index].offset = data_offset;
    entries[index].length = static_cast<std::uint32_t>(std::strlen(payloads[index]));
    entries[index].used = 1;
    data_offset += entries[index].length;
  }
  if (!write_full(fd, entries, sizeof(entries))) {
    syslab_step("索引项没有写完。");
    return syslab_fail("index");
  }
  for (int index = 0; index < 3; ++index) {
    if (!write_full(fd, payloads[index], std::strlen(payloads[index]))) {
      syslab_step("载荷没有写完。");
      return syslab_fail("payload");
    }
  }

  char found[8] = {};
  std::uint32_t length = 0;
  char beta[8] = {};
  set_key(beta, "beta");
  if (!lookup(fd, beta, found, &length) || length != 4 || std::memcmp(found, "BETA", 4) != 0) {
    syslab_step("键 beta 没有落到它自己的载荷。");
    return syslab_fail("hit");
  }
  char missing[8] = {};
  set_key(missing, "delta");
  std::memset(found, 0, sizeof(found));
  length = 0;
  if (lookup(fd, missing, found, &length)) {
    syslab_step("不存在的键 delta 返回了载荷。");
    return syslab_fail("miss");
  }
  ::close(fd);
  syslab_note("OS", "a missing key has no index entry and does not alias an occupied offset");
  syslab_step("键 beta 读到 BETA。键 delta 没有索引项。");
  return syslab_pass();
}
