#include "syslab/memory/size_class_allocator.hpp"

#include "internal.hpp"
#include "syslab/memory/virtual_memory.hpp"

#include <sys/mman.h>

namespace syslab {
namespace {

struct ClassInfo {
  std::size_t size;
  std::size_t alignment;
};

constexpr ClassInfo kClasses[] = {
    {16, 16},   {32, 32},   {48, 16},   {64, 64},   {96, 32},    {128, 64},   {192, 64},
    {256, 64},  {384, 64},  {512, 64},  {768, 64},  {1024, 64},  {2048, 64},  {4096, 64},
};

int select_class(std::size_t size, std::size_t alignment) {
  for (std::size_t index = 0; index < sizeof(kClasses) / sizeof(kClasses[0]); ++index) {
    const ClassInfo& info = kClasses[index];
    if (info.size >= size && alignment <= info.alignment && (info.alignment % alignment) == 0) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

}  // namespace

SizeClassAllocator::SizeClassAllocator(std::size_t blocks_per_class) : blocks_per_class_(blocks_per_class) {
  if (blocks_per_class_ == 0) {
    throw Error(Error::Code::InvalidArgument, "size-class block count must be non-zero");
  }
  classes_.resize(sizeof(kClasses) / sizeof(kClasses[0]));
  for (const ClassInfo& info : kClasses) {
    class_sizes_.push_back(info.size);
  }
  stats_.available_fields = mem::measured_stats() & ~static_cast<std::uint32_t>(StatField::Committed);
}

SizeClassAllocator::~SizeClassAllocator() {
  for (Large& block : large_) {
    if (block.mapping != nullptr) {
      munmap(block.mapping, block.mapping_size);
    }
  }
}

void* SizeClassAllocator::allocate(const AllocationRequest& request) {
  mem::validate_request(request);
  const int index = select_class(request.size, request.alignment);
  if (index >= 0) {
    if (!classes_[static_cast<std::size_t>(index)]) {
      classes_[static_cast<std::size_t>(index)] = std::make_unique<PoolAllocator>(
          kClasses[index].size, blocks_per_class_, kClasses[index].alignment);
    }
    void* pointer = classes_[static_cast<std::size_t>(index)]->allocate(request);
    if (pointer != nullptr) {
      stats_.allocated_bytes += request.size;
      ++stats_.allocation_count;
      mem::note_peak(&stats_);
      return pointer;
    }
  }
  const std::size_t alignment = std::max(request.alignment, alignof(mem::UserHeader));
  const std::size_t prefix = align_up(sizeof(mem::UserHeader), alignment);
  std::size_t total = 0;
  if (add_overflow(prefix, request.size, &total) || add_overflow(total, sizeof(std::uint32_t), &total)) {
    return nullptr;
  }
  MappedRegion mapping(total);
  Large block;
  block.mapping_size = mapping.size();
  block.mapping = mapping.release();
  unsigned char* user = static_cast<unsigned char*>(block.mapping) + prefix;
  auto* header = reinterpret_cast<mem::UserHeader*>(user - sizeof(mem::UserHeader));
  header->magic = mem::kLive;
  header->tag = request.tag;
  header->size = request.size;
  header->alignment = request.alignment;
  header->user_offset = prefix;
  mem::write_canary(user, request.size);
  block.pointer = user;
  block.size = request.size;
  block.alignment = request.alignment;
  large_.push_back(block);
  stats_.allocated_bytes += request.size;
  ++stats_.allocation_count;
  mem::note_peak(&stats_);
  return user;
}

void SizeClassAllocator::deallocate(void* pointer, std::size_t size, std::size_t alignment) {
  if (pointer == nullptr) {
    return;
  }
  for (auto it = large_.begin(); it != large_.end(); ++it) {
    if (it->pointer == pointer) {
      if (it->size != size || it->alignment != alignment) {
        throw Error(Error::Code::Corruption, "large allocation size does not match");
      }
      auto* header = reinterpret_cast<mem::UserHeader*>(static_cast<unsigned char*>(pointer) - sizeof(mem::UserHeader));
      if (header->magic != mem::kLive) {
        throw Error(Error::Code::Corruption, "large allocation header is damaged");
      }
      mem::check_canary(pointer, size);
      munmap(it->mapping, it->mapping_size);
      large_.erase(it);
      stats_.allocated_bytes -= size;
      ++stats_.deallocation_count;
      return;
    }
  }
  const int index = select_class(size, alignment);
  if (index < 0 || !classes_[static_cast<std::size_t>(index)]) {
    throw Error(Error::Code::Corruption, "size-class pointer is not owned by this allocator");
  }
  classes_[static_cast<std::size_t>(index)]->deallocate(pointer, size, alignment);
  stats_.allocated_bytes -= size;
  ++stats_.deallocation_count;
}

AllocationStats SizeClassAllocator::statistics() const {
  AllocationStats stats = stats_;
  stats.reserved_bytes = 0;
  stats.free_bytes = 0;
  for (const auto& pool : classes_) {
    if (!pool) {
      continue;
    }
    const AllocationStats pool_stats = pool->statistics();
    stats.reserved_bytes += pool_stats.reserved_bytes;
    stats.free_bytes += pool_stats.free_bytes;
  }
  for (const Large& block : large_) {
    stats.reserved_bytes += block.mapping_size;
  }
  return stats;
}

}  // namespace syslab
