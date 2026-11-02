#include "experiments/common/check.hpp"
struct Base { virtual int value() const { return 1; } virtual ~Base() = default; };
struct Derived : Base { int value() const override { return 7; } };
// 原理：Base* 指向 Derived 对象时，value() 返回 7。
int main() {
  Derived object;
  Base* pointer = &object;
  const int result = pointer->value();
  syslab_metric("virtual_result", result);
  if (result != 7) {
    syslab_step("基类指针没有调用派生类覆盖函数。");
    return syslab_fail("virtual-dispatch");
  }
  syslab_note("LANG", "Base* points at a Derived object and value() returned 7");
    syslab_step("Base* 指向 Derived 对象，value() 返回 7，调用的是派生类覆盖函数。");
  return syslab_pass();
}
