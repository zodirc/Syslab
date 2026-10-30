#include "syslab/memory/buddy_allocator.hpp"

#include "internal.hpp"
#include "syslab/memory/virtual_memory.hpp"

#include <sys/mman.h>

#include <algorithm>

namespace syslab {
namespace {

std::size_t round_pow2(std::size_t value) {
  std::size_t rounded = 1;
  while (rounded < value) {
    if (rounded > (static_cast<std::size_t>(1) << (sizeof(std::size_t) * 8 - 1))) {
      throw Error(Error::Code::InvalidArgument, "buddy capacity overflows");
    }
    rounded <<= 1;
  }
  return rounded;
}

void recompute_free_lists(const std::vector<std::vector<std::size_t>>& free_lists, std::size_t min_block,
                         AllocationStats* stats) {
  std::size_t free_bytes = 0;
  for (std::size_t order = 0; order < free_lists.size(); ++order) {
    std::size_t block = 0;
    if (mul_overflow(min_block, static_cast<std::size_t>(1) << order, &block)) {
      throw Error(Error::Code::Logic, "buddy block size overflows");
    }
    std::size_t bytes = 0;
    if (mul_overflow(block, free_lists[order].size(), &bytes) ||
        add_overflow(free_bytes, bytes, &free_bytes)) {
      throw Error(Error::Code::Logic, "buddy free-byte counter overflows");
    }
  }
  stats->free_bytes = free_bytes;
}

void erase_offset(std::vector<std::size_t>* list, std::size_t offset) {
  for (auto it = list->begin(); it != list->end(); ++it) {
    if (*it == offset) {
      list->erase(it);
      return;
    }
  }
  throw Error(Error::Code::Corruption, "buddy free list is missing a block");
}

}  // namespace

BuddyAllocator::BuddyAllocator(std::size_t capacity) {
  if (capacity < min_block_) {
    throw Error(Error::Code::InvalidArgument, "buddy capacity is below the minimum block");
  }
  const std::size_t rounded = std::max(round_pow2(capacity), static_cast<std::size_t>(4096));
  MappedRegion mapping(rounded);
  if (mapping.size() != rounded) {
    throw Error(Error::Code::Logic, "buddy mapping was rounded beyond the power-of-two size");
  }
  capacity_ = rounded;
  base_ = mapping.release();
  std::size_t block = min_block_;
  max_order_ = 0;
  while (block < capacity_) {
    block <<= 1;
    ++max_order_;
  }
  if (block != capacity_) {
    throw Error(Error::Code::Logic, "buddy mapping is not a power of two");
  }
  free_lists_.assign(static_cast<std::size_t>(max_order_) + 1, {});
  meta_.assign(capacity_ / min_block_, Meta{});
  meta_[0].order = max_order_;
  meta_[0].free = true;
  meta_[0].present = true;
  free_lists_[static_cast<std::size_t>(max_order_)].push_back(0);
  stats_.reserved_bytes = capacity_;
  stats_.free_bytes = capacity_;
  stats_.available_fields = mem::measured_stats();
}

BuddyAllocator::~BuddyAllocator() {
  if (base_ != nullptr) {
    munmap(base_, capacity_);
  }
}

void* BuddyAllocator::allocate(const AllocationRequest& request) {
  mem::validate_request(request);
  const std::size_t alignment = std::max(request.alignment, alignof(mem::UserHeader));
  const std::size_t prefix = align_up(sizeof(mem::UserHeader), alignment);
  std::size_t total = 0;
  if (add_overflow(prefix, request.size, &total) || add_overflow(total, sizeof(std::uint32_t), &total)) {
    return nullptr;
  }
  int order = 0;
  std::size_t block = min_block_;
  while (block < total) {
    if (order == max_order_) {
      return nullptr;
    }
    block <<= 1;
    ++order;
  }
  int found = -1;
  for (int candidate = order; candidate <= max_order_; ++candidate) {
    if (!free_lists_[static_cast<std::size_t>(candidate)].empty()) {
      found = candidate;
      break;
    }
  }
  if (found < 0) {
    return nullptr;
  }
  std::size_t offset = free_lists_[static_cast<std::size_t>(found)].back();
  free_lists_[static_cast<std::size_t>(found)].pop_back();
  while (found > order) {
    --found;
    const std::size_t half = min_block_ << found;
    const std::size_t buddy = offset + half;
    meta_[buddy / min_block_].order = found;
    meta_[buddy / min_block_].free = true;
    meta_[buddy / min_block_].present = true;
    free_lists_[static_cast<std::size_t>(found)].push_back(buddy);
  }
  Meta& meta = meta_[offset / min_block_];
  meta.order = order;
  meta.free = false;
  meta.present = true;
  meta.user_size = request.size;
  meta.alignment = request.alignment;
  unsigned char* user = static_cast<unsigned char*>(base_) + offset + prefix;
  auto* header = reinterpret_cast<mem::UserHeader*>(user - sizeof(mem::UserHeader));
  header->magic = mem::kLive;
  header->tag = request.tag;
  header->size = request.size;
  header->alignment = request.alignment;
  header->user_offset = prefix;
  mem::write_canary(user, request.size);
  stats_.allocated_bytes += request.size;
  ++stats_.allocation_count;
  recompute_free_lists(free_lists_, min_block_, &stats_);
  mem::note_peak(&stats_);
  return user;
}

void BuddyAllocator::deallocate(void* pointer, std::size_t size, std::size_t alignment) {
  if (pointer == nullptr) {
    return;
  }
  auto* header = reinterpret_cast<mem::UserHeader*>(static_cast<unsigned char*>(pointer) - sizeof(mem::UserHeader));
  if (header->magic != mem::kLive || header->size != size || header->alignment != alignment) {
    throw Error(Error::Code::Corruption, "buddy header does not match the free request");
  }
  mem::check_canary(pointer, size);
  const auto* user = static_cast<unsigned char*>(pointer);
  const auto* base = static_cast<unsigned char*>(base_);
  if (user < base || header->user_offset > static_cast<std::uint64_t>(user - base)) {
    throw Error(Error::Code::Corruption, "buddy pointer is outside the mapping");
  }
  const std::size_t offset = static_cast<std::size_t>(user - base) - static_cast<std::size_t>(header->user_offset);
  if (offset % min_block_ != 0) {
    throw Error(Error::Code::Corruption, "buddy block is misaligned");
  }
  const std::size_t index = offset / min_block_;
  if (index >= meta_.size() || !meta_[index].present || meta_[index].free || meta_[index].user_size != size) {
    throw Error(Error::Code::Corruption, "buddy double free or foreign pointer");
  }
  header->magic = mem::kFree;
  int order = meta_[index].order;
  std::size_t block_offset = offset;
  meta_[index].free = true;
  while (order < max_order_) {
    const std::size_t block = min_block_ << order;
    const std::size_t buddy = block_offset ^ block;
    const std::size_t buddy_index = buddy / min_block_;
    if (buddy_index >= meta_.size() || !meta_[buddy_index].present || !meta_[buddy_index].free ||
        meta_[buddy_index].order != order) {
      break;
    }
    erase_offset(&free_lists_[static_cast<std::size_t>(order)], buddy);
    meta_[buddy_index].present = false;
    meta_[buddy_index].free = false;
    meta_[block_offset / min_block_].present = false;
    meta_[block_offset / min_block_].free = false;
    if (buddy < block_offset) {
      block_offset = buddy;
    }
    ++order;
    meta_[block_offset / min_block_].present = true;
    meta_[block_offset / min_block_].free = true;
    meta_[block_offset / min_block_].order = order;
  }
  meta_[block_offset / min_block_].user_size = 0;
  free_lists_[static_cast<std::size_t>(order)].push_back(block_offset);
  stats_.allocated_bytes -= size;
  ++stats_.deallocation_count;
  recompute_free_lists(free_lists_, min_block_, &stats_);
}

AllocationStats BuddyAllocator::statistics() const { return stats_; }

}  // namespace syslab
