#include "syslab/atomic/epoch.hpp"

#include "syslab/core/error.hpp"

#include <mutex>

namespace syslab {

EpochDomain::Participant* EpochDomain::local() {
  thread_local int index = -1;
  thread_local EpochDomain* owner = nullptr;
  if (index >= 0 && owner == this) {
    return &participants_[index];
  }
  static std::mutex mutex;
  std::lock_guard<std::mutex> guard(mutex);
  for (int candidate = 0; candidate < kMaxThreads; ++candidate) {
    bool expected = false;
    if (participants_[candidate].used.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      index = candidate;
      owner = this;
      return &participants_[candidate];
    }
  }
  throw Error(Error::Code::ResourceLimit, "epoch participant table is full");
}

void EpochDomain::pin() {
  Participant* self = local();
  const std::uint64_t epoch = global_.load(std::memory_order_acquire);
  self->pinned.store(epoch, std::memory_order_release);
}

void EpochDomain::unpin() {
  Participant* self = local();
  self->pinned.store(0, std::memory_order_release);
}

void EpochDomain::retire(void* pointer, void (*deleter)(void*)) {
  if (pointer == nullptr) {
    return;
  }
  if (deleter == nullptr) {
    throw Error(Error::Code::InvalidArgument, "epoch retire requires a deleter");
  }
  Participant* self = local();
  Item item;
  item.pointer = pointer;
  item.deleter = deleter;
  item.epoch = global_.load(std::memory_order_acquire);
  self->deferred.push_back(item);
  try_advance(self);
}

void EpochDomain::try_advance(Participant* self) {
  const std::uint64_t current = global_.load(std::memory_order_acquire);
  for (Participant& participant : participants_) {
    if (!participant.used.load(std::memory_order_acquire)) {
      continue;
    }
    const std::uint64_t pinned = participant.pinned.load(std::memory_order_acquire);
    if (pinned != 0 && pinned < current) {
      break;
    }
  }
  std::uint64_t expected = current;
  global_.compare_exchange_strong(expected, current + 1, std::memory_order_acq_rel, std::memory_order_acquire);
  const std::uint64_t observed = global_.load(std::memory_order_acquire);
  std::vector<Item> kept;
  for (const Item& item : self->deferred) {
    bool blocked = false;
    if (observed <= item.epoch + 1) {
      blocked = true;
    } else {
      for (Participant& participant : participants_) {
        if (!participant.used.load(std::memory_order_acquire)) {
          continue;
        }
        const std::uint64_t pinned = participant.pinned.load(std::memory_order_acquire);
        if (pinned != 0 && pinned <= item.epoch) {
          blocked = true;
          break;
        }
      }
    }
    if (blocked) {
      kept.push_back(item);
    } else {
      item.deleter(item.pointer);
    }
  }
  self->deferred.swap(kept);
}

}  // namespace syslab
