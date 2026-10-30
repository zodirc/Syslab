#include "experiments/common/check.hpp"
#include <cstdint>
// 应用：打开优化之后，调试器里看不到某个局部变量，或者该变量的地址上不再有写入。
// 原理：没有被取地址的值可以由编译器放进寄存器。被取地址的局部变量必须能从内存读回写入的值。本节把 volatile int 设为 7 并取地址，读回结果为 7。Debug 与 Release 的栈帧差异由两次构建的反汇编对照。
int main() {
  volatile int local = 7;
  const int* address = const_cast<const int*>(&local);
  if (address == nullptr || *address != 7) {
    syslab_step("被取地址的局部变量没有读回 7。");
    return syslab_fail("local");
  }
  syslab_metric("local", *address);
#if defined(NDEBUG)
  syslab_note("COMPILER", "NDEBUG is defined for this variant");
#else
  syslab_note("COMPILER", "NDEBUG is not defined for this variant");
#endif
  syslab_note("COMPILER", "frame size differences between Debug and Release are disassembly observations");
    syslab_step("被取地址的局部变量读回 7。优化可以改写没有被取地址的值，不能改掉这个读回结果。");
  return syslab_pass();
}
