#include "experiments/common/check.hpp"
#include "syslab/memory/buddy_allocator.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
// 应用：伙伴分配器上同时存活的几块缓冲互相覆盖。
// 原理：这个实现按 2 的幂切分。八次 24 字节分配各自写入不同的字节模式，读回必须与写入一致。全部释放后，一次 128 字节分配能够成功。映射大小保持为请求的 2 的幂。
int main() {
  syslab::BuddyAllocator buddy(64 * 1024);
  std::vector<void*> pointers;
  for (int index = 0; index < 8; ++index) {
    void* pointer = buddy.allocate({24, 8, 0});
    if (pointer == nullptr) {
    syslab_step("分配返回空指针。");
    return syslab_fail("alloc");
  }
    std::memset(pointer, static_cast<int>(0x10 + index), 24);
    pointers.push_back(pointer);
  }
  for (int index = 0; index < 8; ++index) {
    auto* bytes = static_cast<unsigned char*>(pointers[static_cast<std::size_t>(index)]);
    for (int byte = 0; byte < 24; ++byte) {
      if (bytes[byte] != static_cast<unsigned char>(0x10 + index)) {
    syslab_step("同时存活的块读回了其他块写入的字节。");
    return syslab_fail("overlap");
  }
    }
  }
  for (void* pointer : pointers) buddy.deallocate(pointer, 24, 8);
  if (buddy.allocate({128, 16, 0}) == nullptr) {
    syslab_step("释放后的下一次分配没有按预期完成。");
    return syslab_fail("reuse");
  }
  syslab_note("OBSERVED", "buddy orders are powers of two in this allocator; the mapping size stays the requested power of two");
    syslab_step("八块同时存活的 24 字节区域读回各自写入的字节。全部释放后，128 字节请求分配成功。");
  return syslab_pass();
}
