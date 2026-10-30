#include "experiments/common/check.hpp"
struct Base { virtual int value() const { return 1; } virtual ~Base() = default; };
struct Derived : Base { int value() const override { return 7; } };
// 应用：基类指针调用成员函数，实际执行了基类实现，派生类的覆盖没有生效。
// 原理：虚函数调用按对象的动态类型选择覆盖函数。Base* 指向 Derived 对象时，value() 返回 7。虚表槽位和间接调用指令留在反汇编中。
int main() {
  Derived object;
  Base* pointer = &object;
  const int result = pointer->value();
  syslab_metric("virtual_result", result);
  if (result != 7) {
    syslab_step("基类指针没有调用派生类覆盖函数。");
    return syslab_fail("virtual-dispatch");
  }
  syslab_note("LANG", "a virtual call uses the dynamic type");
  syslab_note("ABI", "the indirect call instruction and vtable layout are ABI observations in the disassembly");
    syslab_step("Base* 指向 Derived 对象，value() 返回 7，调用的是派生类覆盖函数。");
  return syslab_pass();
}
