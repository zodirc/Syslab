#include "syslab/memory/freelist_allocator.hpp"

#include "internal.hpp"
#include "syslab/memory/virtual_memory.hpp"

#include <sys/mman.h>

namespace syslab {
namespace {

constexpr std::size_t kMinSpan = 64;

std::size_t bytes_needed(const AllocationRequest& request) {
  const std::size_t alignment = std::max(request.alignment, alignof(mem::UserHeader));
  std::size_t prefix = align_up(sizeof(mem::UserHeader), alignment);
  std::size_t total = 0;
  if (add_overflow(prefix, request.size, &total) || add_overflow(total, sizeof(std::uint32_t), &total)) {
    throw Error(Error::Code::InvalidArgument, "free-list request size overflows");
  }
  total = align_up(total, 16);
  return std::max(total, kMinSpan);
}

}  // namespace

FreeListAllocator::FreeListAllocator(std::size_t capacity) {
  if (capacity < 4096) {
    throw Error(Error::Code::InvalidArgument, "free-list capacity must be at least 4096 bytes");
  }
  MappedRegion mapping(capacity);
  capacity_ = mapping.size();
  base_ = mapping.release();
  Span span;
  span.offset = 0;
  span.size = capacity_;
  span.free = true;
  spans_.push_back(span);
  stats_.reserved_bytes = capacity_;
  stats_.free_bytes = capacity_;
  stats_.available_fields = mem::measured_stats();
}

FreeListAllocator::~FreeListAllocator() {
  if (base_ != nullptr) {
    munmap(base_, capacity_);
  }
}

void FreeListAllocator::recompute_free() {
  std::size_t free_bytes = 0;
  for (const Span& span : spans_) {
    if (span.free) {
      free_bytes += span.size;
    }
  }
  stats_.free_bytes = free_bytes;
}

void* FreeListAllocator::allocate(const AllocationRequest& request) {
  mem::validate_request(request);
  const std::size_t need = bytes_needed(request);
  std::size_t index = spans_.size();
  for (std::size_t i = 0; i < spans_.size(); ++i) {
    if (spans_[i].free && spans_[i].size >= need) {
      index = i;
      break;
    }
  }
  if (index == spans_.size()) {
    return nullptr;
  }
  if (spans_[index].size >= need + kMinSpan) {
    Span remainder;
    remainder.offset = spans_[index].offset + need;
    remainder.size = spans_[index].size - need;
    remainder.free = true;
    spans_[index].size = need;
    spans_.insert(spans_.begin() + static_cast<std::ptrdiff_t>(index + 1), remainder);
  }
  Span& span = spans_[index];
  const std::size_t alignment = std::max(request.alignment, alignof(mem::UserHeader));
  const std::size_t prefix = align_up(sizeof(mem::UserHeader), alignment);
  const std::size_t user_off = span.offset + prefix;
  if (user_off + request.size + sizeof(std::uint32_t) > span.offset + span.size) {
    throw Error(Error::Code::Logic, "free-list span cannot hold the request");
  }
  unsigned char* user = static_cast<unsigned char*>(base_) + user_off;
  auto* header = reinterpret_cast<mem::UserHeader*>(user - sizeof(mem::UserHeader));
  header->magic = mem::kLive;
  header->tag = request.tag;
  header->size = request.size;
  header->alignment = request.alignment;
  header->user_offset = user_off;
  mem::write_canary(user, request.size);
  span.free = false;
  span.pointer = user;
  span.user_size = request.size;
  span.alignment = request.alignment;
  stats_.allocated_bytes += request.size;
  ++stats_.allocation_count;
  recompute_free();
  mem::note_peak(&stats_);
  return user;
}

void FreeListAllocator::deallocate(void* pointer, std::size_t size, std::size_t alignment) {
  if (pointer == nullptr) {
    return;
  }
  std::size_t index = spans_.size();
  for (std::size_t i = 0; i < spans_.size(); ++i) {
    if (spans_[i].pointer == pointer) {
      index = i;
      break;
    }
  }
  if (index == spans_.size() || spans_[index].free || spans_[index].user_size != size ||
      spans_[index].alignment != alignment) {
    throw Error(Error::Code::Corruption, "free-list pointer does not match a live span");
  }
  auto* header = reinterpret_cast<mem::UserHeader*>(static_cast<unsigned char*>(pointer) - sizeof(mem::UserHeader));
  if (header->magic != mem::kLive || header->size != size || header->alignment != alignment) {
    throw Error(Error::Code::Corruption, "free-list header is damaged");
  }
  mem::check_canary(pointer, size);
  header->magic = mem::kFree;
  spans_[index].free = true;
  spans_[index].pointer = nullptr;
  stats_.allocated_bytes -= size;
  ++stats_.deallocation_count;
  if (index + 1 < spans_.size() && spans_[index + 1].free) {
    spans_[index].size += spans_[index + 1].size;
    spans_.erase(spans_.begin() + static_cast<std::ptrdiff_t>(index + 1));
  }
  if (index > 0 && spans_[index - 1].free) {
    spans_[index - 1].size += spans_[index].size;
    spans_.erase(spans_.begin() + static_cast<std::ptrdiff_t>(index));
  }
  recompute_free();
}

AllocationStats FreeListAllocator::statistics() const { return stats_; }

}  // namespace syslab
