#include "syslab/atomic/hazard_pointer.hpp"

#include "syslab/core/error.hpp"

#include <mutex>
#include <unordered_set>
#include <vector>

namespace syslab {
struct HazardStorage {
  HazardDomain::Record records[HazardDomain::kMaxThreads];
  std::mutex mutex;
};

HazardStorage& storage() {
  static HazardStorage value;
  return value;
}

HazardDomain::Record* HazardDomain::records() { return storage().records; }

HazardDomain::Record* HazardDomain::register_thread() {
  thread_local int index = -1;
  if (index >= 0) {
    return &records()[index];
  }
  std::lock_guard<std::mutex> guard(storage().mutex);
  for (int candidate = 0; candidate < kMaxThreads; ++candidate) {
    bool expected = false;
    if (records()[candidate].active.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      index = candidate;
      return &records()[candidate];
    }
  }
  throw Error(Error::Code::ResourceLimit, "hazard-pointer thread limit exceeded");
}

HazardDomain::Guard HazardDomain::acquire() { return Guard(this, register_thread()); }

std::atomic<void*>& HazardDomain::Guard::slot_at(int slot) {
  if (slot < 0 || slot >= kSlots || record_ == nullptr) {
    throw Error(Error::Code::InvalidArgument, "hazard slot is out of range");
  }
  return record_->slots[slot];
}

void HazardDomain::Guard::clear(int slot) { slot_at(slot).store(nullptr, std::memory_order_release); }

HazardDomain::Guard::~Guard() {
  if (record_ == nullptr) {
    return;
  }
  for (int slot = 0; slot < kSlots; ++slot) {
    record_->slots[slot].store(nullptr, std::memory_order_release);
  }
}

HazardDomain::Guard::Guard(Guard&& other) noexcept : domain_(other.domain_), record_(other.record_) {
  other.domain_ = nullptr;
  other.record_ = nullptr;
}

HazardDomain::Guard& HazardDomain::Guard::operator=(Guard&& other) noexcept {
  if (this != &other) {
    if (record_ != nullptr) {
      for (int slot = 0; slot < kSlots; ++slot) {
        record_->slots[slot].store(nullptr, std::memory_order_release);
      }
    }
    domain_ = other.domain_;
    record_ = other.record_;
    other.domain_ = nullptr;
    other.record_ = nullptr;
  }
  return *this;
}

void HazardDomain::Guard::retire(void* pointer, void (*deleter)(void*)) {
  if (pointer == nullptr) {
    return;
  }
  if (deleter == nullptr || record_ == nullptr) {
    throw Error(Error::Code::InvalidArgument, "hazard retire requires a deleter");
  }
  record_->retired.push_back({pointer, deleter});
  if (record_->retired.size() < static_cast<std::size_t>(kSlots * kMaxThreads)) {
    return;
  }
  std::unordered_set<void*> hazards;
  for (int thread = 0; thread < kMaxThreads; ++thread) {
    Record& other = domain_->records()[thread];
    if (!other.active.load(std::memory_order_acquire)) {
      continue;
    }
    for (int slot = 0; slot < kSlots; ++slot) {
      if (void* hazard = other.slots[slot].load(std::memory_order_seq_cst)) {
        hazards.insert(hazard);
      }
    }
  }
  std::vector<std::pair<void*, void (*)(void*)>> kept;
  for (const auto& item : record_->retired) {
    if (hazards.find(item.first) == hazards.end()) {
      item.second(item.first);
    } else {
      kept.push_back(item);
    }
  }
  record_->retired.swap(kept);
}

}  // namespace syslab
