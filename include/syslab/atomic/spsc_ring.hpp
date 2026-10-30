#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace syslab {

/**
 * @brief Bounded single-producer single-consumer ring.
 * 有界的单生产者单消费者环。
 *
 * The producer owns tail and the consumer owns head. Each side publishes its
 * index with release and reads the other index with acquire. Capacity is a
 * power of two. T must be trivially copyable because a slot is written by one
 * thread and later read by the other without a constructor handshake. One
 * producer and one consumer are required. The algorithm is wait-free for each
 * call. The atomic indices must be lock-free for the progress claim to match
 * the implementation; is_lock_free reports that fact.
 * 生产者拥有 tail，消费者拥有 head。每一侧用 release 公布自己的下标，用 acquire 读取对方的下标。容量是 2 的幂。T 必须可平凡复制，因为槽由一个线程写入，稍后由另一个线程读取，中间没有构造握手。必须恰好一个生产者和一个消费者。每次调用是 wait-free 的。进度声明与实现对得上的前提是原子下标无锁；is_lock_free 报告这一事实。
 */
template <typename T, std::size_t Capacity>
class SpscRing {
  static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "capacity must be a power of two");
  static_assert(std::is_trivially_copyable<T>::value, "SPSC slots are copied as bytes");

 public:
  SpscRing() = default;
  SpscRing(const SpscRing&) = delete;
  SpscRing& operator=(const SpscRing&) = delete;

  /**
   * @brief Pushes one value.
   * 推入一个值。
   * @param value Value copied into the next slot.
   * 复制进下一槽的值。
   * @return False when the ring is full.
   * 环已满时为假。
   */
  bool push(const T& value) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    const std::size_t head = head_.load(std::memory_order_acquire);
    if (tail - head == Capacity) {
      return false;
    }
    slots_[tail & (Capacity - 1)] = value;
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }

  /**
   * @brief Pops one value.
   * 弹出一个值。
   * @param out Receives the value when the ring is not empty.
   * 环非空时接收该值。
   * @return False when the ring is empty.
   * 环为空时为假。
   */
  bool pop(T& out) {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    if (head == tail) {
      return false;
    }
    out = slots_[head & (Capacity - 1)];
    head_.store(head + 1, std::memory_order_release);
    return true;
  }

  /**
   * @brief Reports whether the index atomics are always lock-free.
   * 报告下标原子是否始终无锁。
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
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
  T slots_[Capacity] {};
};

}  // namespace syslab
