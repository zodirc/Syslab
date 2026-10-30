#pragma once

#include "syslab/platform/cpu.hpp"

#include <atomic>

namespace syslab {

/**
 * @brief Queue node for an MCS lock. Each locking thread supplies its own node.
 * MCS 锁的队列节点。每个加锁线程提供自己的节点。
 *
 * The node must remain alive and exclusively owned until unlock returns.
 * 节点在 unlock 返回前必须保持存活，并且只由该线程使用。
 */
struct McsNode {
  std::atomic<McsNode*> next{nullptr};
  std::atomic<bool> waiting{false};
};

/**
 * @brief MCS queue lock.
 * MCS 队列锁。
 *
 * The tail pointer is the queue. A thread publishes its node with an
 * acquire-release exchange. The predecessor stores the successor with release,
 * and the successor waits on its own flag with acquire. Unlock either swings
 * the tail back to null or sets the successor's flag with release. The lock is
 * fair and not reentrant. This is a teaching lock.
 * 尾指针就是队列。线程用 acquire-release 交换公布自己的节点。前驱用 release 存储后继，后继用 acquire 等待自己的标志。解锁或者把尾拨回空，或者用 release 设置后继的标志。锁公平且不可重入。这是教学锁。
 */
class McsLock {
 public:
  McsLock() = default;
  McsLock(const McsLock&) = delete;
  McsLock& operator=(const McsLock&) = delete;

  /**
   * @brief Queues the node and waits until the predecessor hands off.
   * 把节点入队，并等到前驱移交锁。
   * @param node Thread-private node. Must be unused on entry.
   * 线程私有节点。进入时必须未被使用。
   */
  void lock(McsNode& node) noexcept {
    node.next.store(nullptr, std::memory_order_relaxed);
    node.waiting.store(true, std::memory_order_relaxed);
    McsNode* previous = tail_.exchange(&node, std::memory_order_acq_rel);
    if (previous != nullptr) {
      previous->next.store(&node, std::memory_order_release);
      while (node.waiting.load(std::memory_order_acquire)) {
        cpu_relax();
      }
    }
  }

  /**
   * @brief Hands the lock to the successor, or releases the tail if there is none.
   * 把锁交给后继；没有后继时释放尾指针。
   * @param node The same node passed to lock.
   * 传给 lock 的同一个节点。
   */
  void unlock(McsNode& node) noexcept {
    McsNode* next = node.next.load(std::memory_order_acquire);
    if (next == nullptr) {
      McsNode* expected = &node;
      if (tail_.compare_exchange_strong(expected, nullptr, std::memory_order_release, std::memory_order_relaxed)) {
        return;
      }
      while ((next = node.next.load(std::memory_order_acquire)) == nullptr) {
        cpu_relax();
      }
    }
    next->waiting.store(false, std::memory_order_release);
  }

 private:
  std::atomic<McsNode*> tail_{nullptr};
};

/**
 * @brief RAII guard for an MCS lock. The node lives inside the guard.
 * MCS 锁的 RAII 守卫。节点位于守卫内部。
 */
class McsGuard {
 public:
  /**
   * @brief Acquires the lock.
   * 获取锁。
   * @param lock MCS lock.
   * MCS 锁。
   */
  explicit McsGuard(McsLock& lock) : lock_(lock) { lock_.lock(node_); }

  ~McsGuard() { lock_.unlock(node_); }
  McsGuard(const McsGuard&) = delete;
  McsGuard& operator=(const McsGuard&) = delete;

 private:
  McsLock& lock_;
  McsNode node_;
};

}  // namespace syslab
