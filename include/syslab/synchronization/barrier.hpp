#pragma once

#include "syslab/core/error.hpp"
#include "syslab/synchronization/futex.hpp"

#include <atomic>
#include <cstdint>

namespace syslab {

/**
 * @brief Generation barrier for a fixed number of threads.
 * 固定线程数的代数屏障。
 *
 * The arriving thread that completes the generation resets the arrival count
 * and increments the generation with release. Other threads acquire-load the
 * generation and futex-wait while it is unchanged. A wake that lands after the
 * increment observes the new generation and does not sleep. The barrier can be
 * reused. Destroy it only when no thread is inside arrive_and_wait. This is a
 * teaching barrier.
 * 凑满这一代的线程把到达计数清零，并用 release 增加代数。其他线程 acquire 加载代数，并在代数未变时 futex 等待。增量之后才到达的唤醒会看到新代数，因此不会睡眠。屏障可以复用。没有线程停在 arrive_and_wait 内时才能销毁。这是教学屏障。
 */
class Barrier {
 public:
  /**
   * @brief Creates a barrier for a fixed party count.
   * 为固定的参与者数量创建屏障。
   * @param parties Number of threads that must arrive. Must be non-zero.
   * 必须到达的线程数。不能为零。
   * @throws Error InvalidArgument when parties is zero.
   * 参与者数量为零时抛出 Error InvalidArgument。
   */
  explicit Barrier(std::uint32_t parties) : parties_(parties) {
    if (parties_ == 0) {
      throw Error(Error::Code::InvalidArgument, "barrier party count must be non-zero");
    }
  }

  Barrier(const Barrier&) = delete;
  Barrier& operator=(const Barrier&) = delete;

  /**
   * @brief Arrives and waits for the rest of the generation.
   * 到达并等待这一代的其余线程。
   */
  void arrive_and_wait() {
    const std::uint32_t generation = generation_.load(std::memory_order_acquire);
    const std::uint32_t arrived = arrived_.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (arrived == parties_) {
      arrived_.store(0, std::memory_order_relaxed);
      generation_.fetch_add(1, std::memory_order_release);
      futex_wake(&generation_, 1 << 30);
      return;
    }
    while (generation_.load(std::memory_order_acquire) == generation) {
      futex_wait(&generation_, generation);
    }
  }

 private:
  std::uint32_t parties_;
  std::atomic<std::uint32_t> arrived_{0};
  std::atomic<std::uint32_t> generation_{0};
};

}  // namespace syslab
