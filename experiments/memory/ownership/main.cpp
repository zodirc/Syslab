#include "experiments/common/check.hpp"
#include "syslab/runtime/smart_ptr.hpp"
// 应用：对象在仍被使用时析构，或者最后一个使用者离开后对象仍然没有析构。
// 原理：unique 转移后，原指针为空，新指针持有对象。shared 离开作用域后，两个节点都析构。weak 不延长寿命，作用域结束后 expired 为真。
int main() {
  int destroyed = 0;
  struct Node {
    int* counter = nullptr;
    syslab::SharedPtr<Node> next;
    ~Node() { if (counter != nullptr) ++*counter; }
  };
  syslab::WeakPtr<Node> weak;
  {
    auto first = syslab::make_shared<Node>();
    auto second = syslab::make_shared<Node>();
    first->counter = &destroyed;
    second->counter = &destroyed;
    first->next = second;
    weak = syslab::WeakPtr<Node>(second);
    if (!weak.lock()) {
    syslab_step("仍有强引用时，weak 无法锁定对象。");
    return syslab_fail("lock");
  }
  }
  if (destroyed != 2 || !weak.expired()) {
    syslab_step("作用域结束后对象没有析构，或 weak 尚未过期。");
    return syslab_fail("lifetime");
  }
  syslab::UniquePtr<int> owned(new int(4));
  syslab::UniquePtr<int> moved(std::move(owned));
  if (owned.get() != nullptr || moved.get() == nullptr || *moved != 4) {
    syslab_step("unique 转移后的指针关系不正确。");
    return syslab_fail("unique");
  }
  syslab_note("LANG", "weak ownership does not keep the object alive");
    syslab_step("作用域结束时两个节点都析构，weak 已经过期。unique 转移后原指针为空，新指针中的值是 4。");
  return syslab_pass();
}
