#include "experiments/common/check.hpp"
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
// 应用：文件无法作为可执行文件打开，或加载器报告架构不匹配。
// 原理：ELF 文件头的前四个字节是 0x7f、E、L、F。第 5 个字节为 2 表示 ELF64。这是 System V ABI 的文件格式。
int main() {
  std::ifstream input("/proc/self/exe", std::ios::binary);
  if (!input) {
    syslab_step("打不开 /proc/self/exe，本节不能读取本进程的 ELF。");
    return syslab_unsupported("proc-self-exe");
  }
  unsigned char identification[16] = {};
  input.read(reinterpret_cast<char*>(identification), 16);
  if (identification[0] != 0x7f || identification[1] != 'E' || identification[2] != 'L' || identification[3] != 'F') {
    {
    syslab_step("文件头不是 ELF 魔数。");
    return syslab_fail("elf-magic");
  }
  }
  if (identification[4] != 2) {
    syslab_step("文件头的类别字节不是 ELF64。");
    return syslab_unsupported("not-elf64");
  }
  syslab_metric("elf_class", identification[4]);
  syslab_metric("elf_data", identification[5]);
  syslab_note("ABI", "ELF identification is the System V ABI file format, not a C++ language rule");
    syslab_step("本进程可执行文件的前四个字节是 7f、E、L、F，类别字节为 2，与图中的 ELF64 标识一致。");
  return syslab_pass();
}
