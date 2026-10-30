#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace syslab {

/**
 * @brief Fixed-capacity Chase-Lev deque.
 * 固定容量的 Chase-Lev 双端队列。
 *
 * The owner is the only caller of push and pop. Other threads call steal.
 * push fails when the deque is full. The implementation follows the classic
 * bottom/top protocol with a seq_cst fence between the owner's pop store and
 * the top load, and between a thief's top load and bottom load. Capacity is a
 * power of two. This is a teaching deque.
 * 只有拥有者可以调用 push 和 pop。其他线程调用 steal。队列满时 push 失败。实现遵循经典的 bottom/top 协议：拥有者弹出时的存储和读取 top 之间有 seq_cst 栅栏，窃取者读取 top 和读取 bottom 之间也有。容量是 2 的幂。这是教学双端队列。
 */
template <std::size_t Capacity>
class ChaseLevDeque {
  static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "capacity must be a power of two");

 public:
  ChaseLevDeque() = default;
  ChaseLevDeque(const ChaseLevDeque&) = delete;
  ChaseLevDeque& operator=(const ChaseLevDeque&) = delete;

  /**
   * @brief Pushes a value. Only the owner may call this.
   * 推入一个值。只有拥有者可以调用。
   * @param value Non-null task pointer.
   * 非空的任务指针。
   * @return False when the deque is full.
   * 双端队列已满时为假。
   */
  bool push(void* value) {
    const std::int64_t bottom = bottom_.load(std::memory_order_relaxed);
    const std::int64_t top = top_.load(std::memory_order_acquire);
    if (bottom - top >= static_cast<std::int64_t>(Capacity)) {
      return false;
    }
    buffer_[static_cast<std::size_t>(bottom) & (Capacity - 1)].store(value, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    bottom_.store(bottom + 1, std::memory_order_relaxed);
    return true;
  }

  /**
   * @brief Pops the newest value. Only the owner may call this.
   * 弹出最新的值。只有拥有者可以调用。
   * @return The value, or null when the deque is empty or lost a race with a thief.
   * 该值。队列为空或与窃取者竞争失败时为空。
   */
  void* pop() {
    const std::int64_t bottom = bottom_.load(std::memory_order_relaxed) - 1;
    bottom_.store(bottom, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::int64_t top = top_.load(std::memory_order_relaxed);
    if (top <= bottom) {
      void* value = buffer_[static_cast<std::size_t>(bottom) & (Capacity - 1)].load(std::memory_order_relaxed);
      if (top == bottom) {
        if (!top_.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
          value = nullptr;
        }
        bottom_.store(bottom + 1, std::memory_order_relaxed);
      }
      return value;
    }
    bottom_.store(bottom + 1, std::memory_order_relaxed);
    return nullptr;
  }

  /**
   * @brief Steals the oldest value.
   * 窃取最旧的值。
   * @return The value, or null when the deque is empty or the claim fails.
   * 该值。队列为空或认领失败时为空。
   */
  void* steal() {
    std::int64_t top = top_.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    const std::int64_t bottom = bottom_.load(std::memory_order_acquire);
    if (top < bottom) {
      void* value = buffer_[static_cast<std::size_t>(top) & (Capacity - 1)].load(std::memory_order_relaxed);
      if (!top_.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
        return nullptr;
      }
      return value;
    }
    return nullptr;
  }

 private:
  alignas(64) std::atomic<std::int64_t> top_{0};
  alignas(64) std::atomic<std::int64_t> bottom_{0};
  alignas(64) std::atomic<void*> buffer_[Capacity] {};
};

/**
 * @brief Runs a range by pushing per-worker indices and stealing the rest.
 * 把区间按工人切分推入，其余工作通过窃取完成。
 *
 * Each worker owns one deque and is the only thread that pushes to it. After
 * every worker has published its initial slice, workers pop locally and steal
 * from the others. The call returns after every index has been processed.
 * The body may run concurrently and must synchronize its own shared state.
 * 每个工人拥有一个双端队列，并且只有它能向自己的队列推入。每个工人公布初始切片之后，工人本地弹出，并向其他人窃取。每个下标都处理完后调用返回。函数体可能并发运行，必须自己同步共享状态。
 *
 * @param workers Number of kernel threads. Zero is rejected.
 * 内核线程数。零会被拒绝。
 * @param count Number of indices in [0, count).
 * 区间 [0, count) 中的下标个数。
 * @param body Function invoked once per index.
 * 每个下标调用一次的函数。
 * @throws Error InvalidArgument when workers is zero.
 * workers 为零时抛出 Error InvalidArgument。
 * @throws Error ResourceLimit when a worker deque cannot hold its initial slice.
 * 工人的双端队列放不下初始切片时抛出 Error ResourceLimit。
 */
void parallel_for(unsigned workers, std::size_t count, const std::function<void(std::size_t)>& body);

}  // namespace syslab
