#pragma once

#include <atomic>
#include <cstdint>
#include <utility>
#include <vector>

namespace syslab {

/**
 * @brief Epoch-based reclamation domain.
 * 基于纪元的回收域。
 *
 * pin publishes the current global epoch. unpin publishes a quiescent state.
 * retire keeps an object until a later epoch is observed and every pinned
 * participant has either left or entered with an epoch at least as new as the
 * retirement epoch. Deleters run on the retiring thread. At most 64 threads
 * may pin concurrently. This is a teaching collector.
 * pin 公布当前全局纪元。unpin 公布静止状态。retire 保留对象，直到观察到更晚的纪元，并且每个已 pin 的参与者要么已离开，要么带着不早于退役纪元的纪元进入。删除函数在退役线程上运行。最多 64 个线程可以同时 pin。这是教学回收器。
 */
class EpochDomain {
 public:
  static constexpr int kMaxThreads = 64;

  EpochDomain() = default;
  EpochDomain(const EpochDomain&) = delete;
  EpochDomain& operator=(const EpochDomain&) = delete;

  /**
   * @brief Enters a read-side critical section.
   * 进入读端临界区。
   * @throws Error ResourceLimit when the participant table is full.
   * 参与者表已满时抛出 Error ResourceLimit。
   */
  void pin();

  /**
   * @brief Leaves the read-side critical section.
   * 离开读端临界区。
   */
  void unpin();

  /**
   * @brief Defers deletion until a grace period has elapsed.
   * 把删除推迟到宽限期结束。
   * @param pointer Object to delete.
   * 要删除的对象。
   * @param deleter Function that deletes the object.
   * 删除该对象的函数。
   */
  void retire(void* pointer, void (*deleter)(void*));

 private:
  struct Item {
    void* pointer = nullptr;
    void (*deleter)(void*) = nullptr;
    std::uint64_t epoch = 0;
  };
  struct Participant {
    std::atomic<std::uint64_t> pinned{0};
    std::atomic<bool> used{false};
    std::vector<Item> deferred;
  };

  Participant* local();
  void try_advance(Participant* self);

  std::atomic<std::uint64_t> global_{1};
  Participant participants_[kMaxThreads];
};

}  // namespace syslab
