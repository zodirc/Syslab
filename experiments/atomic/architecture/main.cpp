#include "experiments/common/check.hpp"
// 应用：把这台机器上观察到的原子指令形式写成所有架构的结论。
// 原理：当前二进制只为一种 ISA 编译。x86-64 与 AArch64 的宏恰好一个成立。其他架构返回不支持。一次运行不能对照另一套指令集。
int main() {
#if defined(__x86_64__)
  syslab_note("ISA", "this binary targets x86-64 TSO; it does not execute AArch64");
  syslab_metric("arch", 1);
#elif defined(__aarch64__)
  syslab_note("ISA", "this binary targets AArch64; it does not execute x86-64");
  syslab_metric("arch", 2);
#else
  {
    syslab_step("当前二进制不是 x86-64 或 AArch64。");
    return syslab_unsupported("architecture");
  }
#endif
  syslab_note("LANG", "seq_cst constraints in AT-004 do not become weaker on either architecture");
    syslab_step("当前二进制只命中一种 ISA 宏。这次运行不能推广到另一种指令集。");
  return syslab_pass();
}
