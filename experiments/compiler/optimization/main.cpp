#include "experiments/common/check.hpp"
static int sink(int value) { return value; }
static int constant_result() {
  const int left = 2;
  const int right = 3;
  if (left == 0) return sink(right);
  return left * right;
}
// 应用：优化前后返回值相同，但怀疑编译器改写了源码中的乘法和不会执行的分支。
// 原理：抽象机把 constant_result 求值为 6。常量折叠和删除不可达分支是编译器允许的改写，是否发生只出现在反汇编对照里。
int main() {
  const int value = constant_result();
  syslab_metric("result", value);
  if (value != 6) {
    syslab_step("返回值不是抽象机规定的 6。");
    return syslab_fail("result");
  }
  syslab_note("LANG", "the abstract machine evaluates this function to 6");
  syslab_note("COMPILER", "constant folding and dead-code elimination are permitted and visible only by comparing disassembly");
    syslab_step("constant_result() 的返回值是 6。未优化和优化两次构建都保持这个抽象机结果。");
  return syslab_pass();
}
