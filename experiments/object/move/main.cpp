#include "experiments/common/check.hpp"
#include <utility>

struct Holder {
  int* value = nullptr;

  explicit Holder(int number) : value(new int(number)) {}
  Holder(const Holder&) = delete;
  Holder& operator=(const Holder&) = delete;
  Holder(Holder&& other) noexcept : value(other.value) { other.value = nullptr; }
  Holder& operator=(Holder&&) = delete;
  ~Holder() { delete value; }
};

// 背景：对象持有一块只能有一个所有者的内存。复制会形成两个所有者，移动把所有权交给新对象。
// 语言：移动构造函数从源对象取出指针，并把源对象的指针置空。源对象析构时不会释放已经转走的内存。
int main() {
  Holder source(4);
  int* const address = source.value;
  Holder destination(std::move(source));
  if (source.value != nullptr) {
    syslab_step("移动之后源对象仍持有指针。");
    return syslab_fail("source-empty");
  }
  if (destination.value != address || destination.value == nullptr || *destination.value != 4) {
    syslab_step("新对象没有接到原来的值和原来的地址。");
    return syslab_fail("destination");
  }
  syslab_step("移动构造之后，源对象的指针为空。");
  syslab_step("新对象持有原来的地址，值仍是 4。");
  return syslab_pass();
}
