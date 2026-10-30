#include "syslab/memory/arena_allocator.hpp"

#include "internal.hpp"
#include "syslab/memory/virtual_memory.hpp"

#include <sys/mman.h>

namespace syslab {
namespace {

void release_mapping(void* base, std::size_t size) {
  if (base != nullptr && size != 0) {
    munmap(base, size);
  }
}

}  // namespace

ArenaAllocator::ArenaAllocator(std::size_t capacity) {
  if (capacity < 4096) {
    throw Error(Error::Code::InvalidArgument, "arena capacity must be at least 4096 bytes");
  }
  MappedRegion mapping(capacity);
  capacity_ = mapping.size();
  base_ = mapping.release();
  stats_.reserved_bytes = capacity_;
  stats_.free_bytes = capacity_;
  stats_.available_fields = mem::measured_stats();
  std::size_t resident = 0;
  if (count_resident_pages(base_, capacity_, &resident)) {
    stats_.committed_bytes = resident * system_page_size();
    stats_.available_fields |= static_cast<std::uint32_t>(StatField::Committed);
  }
}

ArenaAllocator::~ArenaAllocator() { release_mapping(base_, capacity_); }

void* ArenaAllocator::allocate(const AllocationRequest& request) {
  mem::validate_request(request);
  const std::size_t alignment = std::max(request.alignment, alignof(mem::UserHeader));
  std::size_t header_at = 0;
  if (add_overflow(offset_, sizeof(mem::UserHeader), &header_at)) {
    return nullptr;
  }
  std::size_t user_off = 0;
  try {
    user_off = align_up(header_at, alignment);
  } catch (const Error&) {
    return nullptr;
  }
  std::size_t canary_off = 0;
  std::size_t end = 0;
  if (add_overflow(user_off, request.size, &canary_off) ||
      add_overflow(canary_off, sizeof(std::uint32_t), &end)) {
    return nullptr;
  }
  try {
    end = align_up(end, 16);
  } catch (const Error&) {
    return nullptr;
  }
  if (end > capacity_) {
    return nullptr;
  }
  auto* header = reinterpret_cast<mem::UserHeader*>(static_cast<unsigned char*>(base_) + user_off -
                                                    sizeof(mem::UserHeader));
  header->magic = mem::kLive;
  header->tag = request.tag;
  header->size = request.size;
  header->alignment = request.alignment;
  header->user_offset = user_off;
  void* user = static_cast<unsigned char*>(base_) + user_off;
  mem::write_canary(user, request.size);
  Record record;
  record.pointer = user;
  record.size = request.size;
  record.alignment = request.alignment;
  record.begin = user_off - sizeof(mem::UserHeader);
  record.end = end;
  record.live = true;
  records_.push_back(record);
  offset_ = end;
  stats_.allocated_bytes += request.size;
  ++stats_.allocation_count;
  stats_.free_bytes = capacity_ - offset_;
  mem::note_peak(&stats_);
  return user;
}

void ArenaAllocator::deallocate(void* pointer, std::size_t size, std::size_t alignment) {
  if (pointer == nullptr) {
    return;
  }
  Record* record = nullptr;
  for (Record& candidate : records_) {
    if (candidate.pointer == pointer) {
      record = &candidate;
      break;
    }
  }
  if (record == nullptr || record->size != size || record->alignment != alignment) {
    throw Error(Error::Code::Corruption, "arena pointer does not match a live block");
  }
  if (!record->live) {
    throw Error(Error::Code::Corruption, "arena double free");
  }
  auto* header = reinterpret_cast<mem::UserHeader*>(static_cast<unsigned char*>(pointer) - sizeof(mem::UserHeader));
  if (header->magic != mem::kLive || header->size != size || header->alignment != alignment) {
    throw Error(Error::Code::Corruption, "arena header is damaged");
  }
  mem::check_canary(pointer, size);
  header->magic = mem::kFree;
  record->live = false;
  stats_.allocated_bytes -= size;
  ++stats_.deallocation_count;
  while (!records_.empty() && !records_.back().live) {
    offset_ = records_.back().begin;
    records_.pop_back();
  }
  stats_.free_bytes = capacity_ - offset_;
}

AllocationStats ArenaAllocator::statistics() const {
  AllocationStats stats = stats_;
  std::size_t resident = 0;
  if (count_resident_pages(base_, capacity_, &resident)) {
    stats.committed_bytes = resident * system_page_size();
    stats.available_fields |= static_cast<std::uint32_t>(StatField::Committed);
  }
  return stats;
}

void ArenaAllocator::reset() {
  offset_ = 0;
  records_.clear();
  stats_.allocated_bytes = 0;
  stats_.free_bytes = capacity_;
}

}  // namespace syslab
