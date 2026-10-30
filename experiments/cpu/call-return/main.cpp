#include "experiments/common/check.hpp"
static long add_six(long a, long b, long c, long d, long e, long f) { return a + b + c + d + e + f; }
// 应用：函数返回的合计与调用方传入的参数之和不一致，或崩溃发生在函数返回之后。
// 原理：语言要求 add_six(1, 2, 3, 4, 5, 6) 的返回值等于 21。参数进入哪个寄存器属于调用约定，由反汇编观察，不作为本节的数值条件。
int main() {
  const long sum = add_six(1, 2, 3, 4, 5, 6);
  syslab_metric("sum", sum);
  if (sum != 21) {
    syslab_step("求和结果与本节预期不一致。");
    return syslab_fail("sum");
  }
  syslab_note("LANG", "the C++ abstract machine requires this function to return 21");
  syslab_note("ABI", "which registers carry the arguments is an ABI fact observed in the disassembly, not checked by opcode");
    syslab_step("add_six(1, 2, 3, 4, 5, 6) 的返回值是 21，与图中的返回值一致。");
  return syslab_pass();
}
