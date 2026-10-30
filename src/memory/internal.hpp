#pragma once

#include "syslab/core/check.hpp"
#include "syslab/core/error.hpp"
#include "syslab/memory/allocator.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace syslab {
namespace mem {

constexpr std::uint32_t kLive = 0xA11C0A11u;
constexpr std::uint32_t kFree = 0xF4EEDEADu;
constexpr std::uint32_t kCanary = 0xC0A5C0A5u;

struct alignas(16) UserHeader {
  std::uint32_t magic;
  std::uint32_t tag;
  std::uint64_t size;
  std::uint64_t alignment;
  std::uint64_t user_offset;
};

static_assert(sizeof(UserHeader) == 32, "UserHeader must stay 32 bytes");

inline constexpr std::uint32_t measured_stats() noexcept {
  return static_cast<std::uint32_t>(StatField::Reserved) | static_cast<std::uint32_t>(StatField::Allocated) |
         static_cast<std::uint32_t>(StatField::Free) |
         static_cast<std::uint32_t>(StatField::AllocationCount) |
         static_cast<std::uint32_t>(StatField::DeallocationCount) |
         static_cast<std::uint32_t>(StatField::PeakAllocated);
}

inline void validate_request(const AllocationRequest& request) {
  if (request.size == 0) {
    throw Error(Error::Code::InvalidArgument, "allocation size must be non-zero");
  }
  if (!is_power_of_two(request.alignment)) {
    throw Error(Error::Code::InvalidArgument, "allocation alignment must be a power of two");
  }
}

inline void write_canary(void* user, std::size_t size) {
  std::memcpy(static_cast<unsigned char*>(user) + size, &kCanary, sizeof(kCanary));
}

inline void check_canary(const void* user, std::size_t size) {
  std::uint32_t value = 0;
  std::memcpy(&value, static_cast<const unsigned char*>(user) + size, sizeof(value));
  if (value != kCanary) {
    throw Error(Error::Code::Corruption, "allocation canary is damaged");
  }
}

inline void note_peak(AllocationStats* stats) {
  if (stats->allocated_bytes > stats->peak_allocated_bytes) {
    stats->peak_allocated_bytes = stats->allocated_bytes;
  }
}

}  // namespace mem
}  // namespace syslab
