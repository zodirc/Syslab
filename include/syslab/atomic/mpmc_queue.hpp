#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace syslab {

/**
 * @brief Bounded multi-producer multi-consumer queue.
 * 有界的多生产者多消费者队列。
 *
 * Each cell has a sequence number. A producer claims a position, writes the
 * value, then publishes sequence = position + 1 with release. A consumer
 * claims a position only after observing that sequence with acquire. The
 * linearization point of a successful push or pop is the successful
 * compare-exchange of the position. Capacity is a power of two. T must be
 * trivially copyable. The queue is lock-free when the atomic operations are
 * lock-free: a failed call means another thread completed a position claim.
 * This is a teaching queue.
 * 每个单元有一个序号。生产者认领位置、写入值，然后用 release 公布 sequence = position + 1。消费者只有在用 acquire 观察到该序号后才认领位置。成功的 push 或 pop 的线性化点是位置上成功的比较交换。容量是 2 的幂。T 必须可平凡复制。原子操作无锁时队列无锁：调用失败意味着另一个线程完成了位置认领。这是教学队列。
 */
template <typename T, std::size_t Capacity>
class MpmcQueue {
  static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "capacity must be a power of two");
  static_assert(std::is_trivially_copyable<T>::value, "MPMC cells are copied as values");

 public:
  MpmcQueue() {
    for (std::size_t index = 0; index < Capacity; ++index) {
      cells_[index].sequence.store(index, std::memory_order_relaxed);
    }
  }
  MpmcQueue(const MpmcQueue&) = delete;
  MpmcQueue& operator=(const MpmcQueue&) = delete;

  /**
   * @brief Enqueues a value.
   * 入队一个值。
   * @param value Value copied into a claimed cell.
   * 复制进已认领单元的值。
   * @return False when the queue is full.
   * 队列已满时为假。
   */
  bool push(const T& value) {
    std::size_t position = enqueue_.load(std::memory_order_relaxed);
    for (;;) {
      Cell& cell = cells_[position & (Capacity - 1)];
      const std::size_t sequence = cell.sequence.load(std::memory_order_acquire);
      const std::intptr_t difference = static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(position);
      if (difference == 0) {
        if (enqueue_.compare_exchange_weak(position, position + 1, std::memory_order_relaxed)) {
          cell.value = value;
          cell.sequence.store(position + 1, std::memory_order_release);
          return true;
        }
      } else if (difference < 0) {
        return false;
      } else {
        position = enqueue_.load(std::memory_order_relaxed);
      }
    }
  }

  /**
   * @brief Dequeues a value.
   * 出队一个值。
   * @param out Receives the value when a cell is claimed.
   * 认领到单元时接收该值。
   * @return False when the queue is empty.
   * 队列为空时为假。
   */
  bool pop(T& out) {
    std::size_t position = dequeue_.load(std::memory_order_relaxed);
    for (;;) {
      Cell& cell = cells_[position & (Capacity - 1)];
      const std::size_t sequence = cell.sequence.load(std::memory_order_acquire);
      const std::intptr_t difference =
          static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(position + 1);
      if (difference == 0) {
        if (dequeue_.compare_exchange_weak(position, position + 1, std::memory_order_relaxed)) {
          out = cell.value;
          cell.sequence.store(position + Capacity, std::memory_order_release);
          return true;
        }
      } else if (difference < 0) {
        return false;
      } else {
        position = dequeue_.load(std::memory_order_relaxed);
      }
    }
  }

  /**
   * @brief Reports whether the position atomics are always lock-free.
   * 报告位置原子是否始终无锁。
   *
   * The progress claim requires this. The check is the compiler builtin, so it
   * does not depend on libatomic.
   * 进度声明依赖这一点。检查使用编译器内建函数，因此不依赖 libatomic。
   *
   * @return True when a size_t atomic is always lock-free.
   * size_t 原子始终无锁时为真。
   */
  bool is_lock_free() const noexcept { return __atomic_always_lock_free(sizeof(std::size_t), nullptr); }

 private:
  struct Cell {
    std::atomic<std::size_t> sequence{0};
    T value{};
  };

  alignas(64) Cell cells_[Capacity];
  alignas(64) std::atomic<std::size_t> enqueue_{0};
  alignas(64) std::atomic<std::size_t> dequeue_{0};
};

}  // namespace syslab
