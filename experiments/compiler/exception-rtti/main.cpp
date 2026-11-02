#include "experiments/common/check.hpp"
#include <typeinfo>
struct Base { virtual ~Base() = default; };
struct Derived : Base {};
// 原理：throw Derived() 之后，catch (const Base&) 接到对象。typeid(object) 与 typeid(Derived) 相等。
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
  syslab_note("LANG", "catch (const Base&) received the object and typeid matched Derived");
    syslab_step("catch (const Base&) 捕获 Derived，typeid(object) 与 typeid(Derived) 相等。");
  return syslab_pass();
}
