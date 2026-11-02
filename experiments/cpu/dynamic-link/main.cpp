#include "experiments/common/check.hpp"
#include <cstdint>
#include <fstream>
#include <vector>
// 原理：小端 ELF64 的程序头里存在类型为 3 的 PT_INTERP。没有这一项时，结果为不支持。
int main() {
  std::ifstream input("/proc/self/exe", std::ios::binary);
  if (!input) {
    syslab_step("打不开 /proc/self/exe，本节不能读取本进程的 ELF。");
    return syslab_unsupported("proc-self-exe");
  }
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), {});
  if (bytes.size() < 64 || bytes[0] != 0x7f) {
    syslab_step("文件头不是 ELF 魔数。");
    return syslab_fail("elf-magic");
  }
  const unsigned char klass = bytes[4];
  const unsigned char data = bytes[5];
  if (klass != 2 || data != 1) {
    syslab_step("文件不是小端 ELF64。");
    return syslab_unsupported("not-elf64-le");
  }
  auto read64 = [&](std::size_t offset) {
    std::uint64_t value = 0;
    for (int byte = 7; byte >= 0; --byte) value = (value << 8) | bytes[offset + static_cast<std::size_t>(byte)];
    return value;
  };
  auto read16 = [&](std::size_t offset) -> std::uint16_t {
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
  };
  const std::uint64_t phoff = read64(32);
  const std::uint16_t phentsize = read16(54);
  const std::uint16_t phnum = read16(56);
  bool interpreter = false;
  for (std::uint16_t index = 0; index < phnum; ++index) {
    const std::size_t entry = static_cast<std::size_t>(phoff) + static_cast<std::size_t>(index) * phentsize;
    if (entry + 4 > bytes.size()) {
    syslab_step("程序头超出文件范围。");
    return syslab_fail("phdr");
  }
    const std::uint32_t type = static_cast<std::uint32_t>(bytes[entry] | (bytes[entry + 1] << 8) |
                                                         (bytes[entry + 2] << 16) | (bytes[entry + 3] << 24));
    if (type == 3) interpreter = true;
  }
  if (!interpreter) {
    syslab_step("程序头中没有 PT_INTERP，本节不能观察动态加载器。");
    return syslab_unsupported("no-pt-interp");
  }
  syslab_note("ABI", "a program header of type 3 is PT_INTERP");
    syslab_step("程序头中存在类型为 3 的 PT_INTERP。");
  return syslab_pass();
}
