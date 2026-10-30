#pragma once

#include "syslab/atomic/hazard_pointer.hpp"

#include <atomic>
#include <memory>
#include <utility>

namespace syslab {

/**
 * @brief Treiber stack reclaimed with hazard pointers.
 * 用危险指针回收的 Treiber 栈。
 *
 * push links a node with a release compare-exchange. pop protects the current
 * top, then swings top to next with acquire. The successful compare-exchange
 * is the linearization point. A popped node is retired and deleted only when
 * no hazard slot still references it, which closes the ABA window caused by
 * immediate reuse of the node address. The stack is lock-free when the top
 * pointer's atomic is lock-free: a thread loops only when another thread
 * changes top. It is not wait-free. T must be nothrow movable into the node.
 * push 用 release 比较交换链接节点。pop 保护当前栈顶，再用 acquire 把栈顶拨到 next。成功的比较交换是线性化点。弹出的节点被退役，并且只有没有任何危险槽仍引用它时才删除，从而关掉节点地址立刻复用造成的 ABA 窗口。栈顶指针的原子无锁时，栈是 lock-free 的：线程只在另一个线程改变栈顶时循环。它不是 wait-free。T 必须能无抛出地移入节点。
 */
template <typename T>
class LockFreeStack {
  struct Node {
    T value;
    Node* next = nullptr;
  };

 public:
  LockFreeStack() = default;
  LockFreeStack(const LockFreeStack&) = delete;
  LockFreeStack& operator=(const LockFreeStack&) = delete;

  ~LockFreeStack() {
    T value;
    while (pop(value)) {
    }
  }

  /**
   * @brief Pushes a value.
   * 推入一个值。
   * @param value Value moved into a new node.
   * 移入新节点的值。
   */
  void push(T value) {
    auto node = std::unique_ptr<Node>(new Node{std::move(value), nullptr});
    Node* expected = top_.load(std::memory_order_relaxed);
    do {
      node->next = expected;
    } while (!top_.compare_exchange_weak(expected, node.get(), std::memory_order_release,
                                         std::memory_order_relaxed));
    node.release();
  }

  /**
   * @brief Pops a value.
   * 弹出一个值。
   * @param out Receives the popped value.
   * 接收弹出的值。
   * @return False when the stack is empty.
   * 栈为空时为假。
   */
  bool pop(T& out) {
    HazardDomain::Guard guard = hazards_.acquire();
    while (true) {
      Node* head = guard.protect(0, top_);
      if (head == nullptr) {
        return false;
      }
      Node* next = head->next;
      if (top_.compare_exchange_weak(head, next, std::memory_order_acquire, std::memory_order_relaxed)) {
        out = std::move(head->value);
        guard.clear(0);
        guard.retire(head, destroy);
        return true;
      }
    }
  }

  /**
   * @brief Reports whether the top pointer is always lock-free.
   * 报告栈顶指针是否始终无锁。
   *
   * Uses the compiler builtin rather than std::atomic::is_lock_free. The latter
   * can require libatomic, which is not installed as a shared library on every
   * host. A false result means the progress claim for this stack does not hold.
   * 使用编译器内建函数，而不是 std::atomic::is_lock_free。后者可能需要 libatomic，而并非每台主机都把 libatomic 安装成共享库。结果为假表示这个栈的进度声明不成立。
   *
   * @return True when a pointer-sized atomic is always lock-free.
   * 指针宽度的原子始终无锁时为真。
   */
  bool is_lock_free() const noexcept { return __atomic_always_lock_free(sizeof(Node*), nullptr); }

 private:
  static void destroy(void* pointer) { delete static_cast<Node*>(pointer); }

  std::atomic<Node*> top_{nullptr};
  HazardDomain hazards_;
};

}  // namespace syslab
