#include "experiments/common/check.hpp"
#include <typeinfo>
struct Base { virtual ~Base() = default; };
struct Derived : Base {};
// 应用：catch 接到了异常对象，但类型判断把派生类当成了基类本身。
// 原理：catch (const Base&) 可以捕获 Derived。处理函数参数的静态类型是 Base，typeid(object) 给出动态类型 Derived。异常表和 typeinfo 的符号布局属于 ABI，本节不比对符号名。
int main() {
  bool caught = false;
  try {
    throw Derived();
  } catch (const Base& object) {
    caught = typeid(object) == typeid(Derived);
  }
  if (!caught) {
    syslab_step("catch 接到对象后，动态类型不是派生类。");
    return syslab_fail("dynamic-type");
  }
  syslab_note("LANG", "the handler matches the base type");
  syslab_note("ABI", "unwind tables and typeinfo symbols are implementation details left to the binary observation");
    syslab_step("catch (const Base&) 捕获 Derived，typeid 与 Derived 相等。静态类型和动态类型因此分开。");
  return syslab_pass();
}
