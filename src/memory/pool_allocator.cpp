#include "syslab/memory/pool_allocator.hpp"

#include "internal.hpp"
#include "syslab/memory/virtual_memory.hpp"

#include <sys/mman.h>

namespace syslab {
namespace {

constexpr std::uint32_t kEnd = 0xffffffffu;

}  // namespace

PoolAllocator::PoolAllocator(std::size_t block_size, std::size_t block_count, std::size_t alignment) {
  if (block_size == 0 || block_count == 0) {
    throw Error(Error::Code::InvalidArgument, "pool block size and count must be non-zero");
  }
  if (!is_power_of_two(alignment)) {
    throw Error(Error::Code::InvalidArgument, "pool alignment must be a power of two");
  }
  block_size_ = block_size;
  alignment_ = alignment;
  user_delta_ = align_up(sizeof(mem::UserHeader), std::max(alignment, alignof(mem::UserHeader)));
  std::size_t stride = 0;
  if (add_overflow(user_delta_, block_size, &stride) || add_overflow(stride, sizeof(std::uint32_t), &stride)) {
    throw Error(Error::Code::InvalidArgument, "pool block size overflows");
  }
  stride_ = align_up(stride, std::max(alignment, static_cast<std::size_t>(16)));
  std::size_t bytes = 0;
  if (mul_overflow(stride_, block_count, &bytes)) {
    throw Error(Error::Code::InvalidArgument, "pool mapping size overflows");
  }
  MappedRegion mapping(bytes);
  mapping_size_ = mapping.size();
  base_ = mapping.release();
  blocks_.resize(block_count);
  free_head_ = 0;
  for (std::uint32_t index = 0; index < block_count; ++index) {
    blocks_[index].next = index + 1;
    blocks_[index].in_list = true;
    blocks_[index].live = false;
  }
  blocks_.back().next = kEnd;
  stats_.reserved_bytes = mapping_size_;
  stats_.free_bytes = block_count * block_size_;
  stats_.available_fields = mem::measured_stats();
  free_count_ = block_count;
}

PoolAllocator::~PoolAllocator() {
  if (base_ != nullptr) {
    munmap(base_, mapping_size_);
  }
}

void* PoolAllocator::allocate(const AllocationRequest& request) {
  mem::validate_request(request);
  if (request.size > block_size_ || request.alignment > alignment_ || (alignment_ % request.alignment) != 0) {
    return nullptr;
  }
  if (free_head_ == kEnd) {
    return nullptr;
  }
  const std::uint32_t index = free_head_;
  Block& block = blocks_[index];
  if (block.live || !block.in_list) {
    throw Error(Error::Code::Corruption, "pool free list is damaged");
  }
  free_head_ = block.next;
  block.in_list = false;
  block.live = true;
  block.next = kEnd;
  unsigned char* user = static_cast<unsigned char*>(base_) + static_cast<std::size_t>(index) * stride_ + user_delta_;
  auto* header = reinterpret_cast<mem::UserHeader*>(user - sizeof(mem::UserHeader));
  header->magic = mem::kLive;
  header->tag = request.tag;
  header->size = request.size;
  header->alignment = request.alignment;
  header->user_offset = index;
  mem::write_canary(user, request.size);
  stats_.allocated_bytes += request.size;
  ++stats_.allocation_count;
  --free_count_;
  stats_.free_bytes = free_count_ * block_size_;
  mem::note_peak(&stats_);
  return user;
}

void PoolAllocator::deallocate(void* pointer, std::size_t size, std::size_t alignment) {
  if (pointer == nullptr) {
    return;
  }
  auto* user = static_cast<unsigned char*>(pointer);
  auto* base = static_cast<unsigned char*>(base_);
  if (user < base || static_cast<std::size_t>(user - base) < user_delta_) {
    throw Error(Error::Code::Corruption, "pool pointer is outside the mapping");
  }
  const std::size_t relative = static_cast<std::size_t>(user - base);
  if (relative % stride_ != user_delta_) {
    throw Error(Error::Code::Corruption, "pool pointer is not a block address");
  }
  const std::size_t index = relative / stride_;
  if (index >= blocks_.size()) {
    throw Error(Error::Code::Corruption, "pool pointer is outside the block table");
  }
  Block& block = blocks_[index];
  if (!block.live || block.in_list) {
    throw Error(Error::Code::Corruption, "pool double free");
  }
  auto* header = reinterpret_cast<mem::UserHeader*>(user - sizeof(mem::UserHeader));
  if (header->magic != mem::kLive || header->size != size || header->alignment != alignment ||
      header->user_offset != index) {
    throw Error(Error::Code::Corruption, "pool header does not match the free request");
  }
  mem::check_canary(user, size);
  header->magic = mem::kFree;
  block.live = false;
  block.in_list = true;
  block.next = free_head_;
  free_head_ = static_cast<std::uint32_t>(index);
  stats_.allocated_bytes -= size;
  ++stats_.deallocation_count;
  ++free_count_;
  stats_.free_bytes = free_count_ * block_size_;
}

AllocationStats PoolAllocator::statistics() const { return stats_; }

}  // namespace syslab
