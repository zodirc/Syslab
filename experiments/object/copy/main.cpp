#include "experiments/common/check.hpp"

// 背景：含指针的对象被复制后，修改其中一份会不会改到另一份，取决于复制的是指针还是所指内容。
// 语言：默认复制指针成员时，两个对象持有同一地址。深拷贝为新对象分配另一块内存并复制值。
int main() {
  int storage = 7;
  int* source = &storage;
  int* shallow = source;
  if (shallow != source) {
    syslab_step("浅拷贝没有复制指针。");
    return syslab_fail("shallow-identity");
  }
  *source = 9;
  if (*shallow != 9) {
    syslab_step("修改源对象后，浅拷贝没有看到同一个值。");
    return syslab_fail("shallow-alias");
  }
  int* deep_source = new int(7);
  int* deep_copy = new int(*deep_source);
  if (deep_copy == deep_source || *deep_copy != 7) {
    delete deep_source;
    delete deep_copy;
    syslab_step("深拷贝没有得到另一块相同的值。");
    return syslab_fail("deep-copy");
  }
  *deep_source = 9;
  const bool isolated = *deep_copy == 7;
  delete deep_source;
  delete deep_copy;
  if (!isolated) {
    syslab_step("修改源对象后，深拷贝的值也变了。");
    return syslab_fail("deep-isolate");
  }
  syslab_step("浅拷贝与源对象是同一地址。源对象改为 9 后，浅拷贝也是 9。");
  syslab_step("深拷贝是另一地址。源对象改为 9 后，深拷贝仍是 7。");
  return syslab_pass();
}
