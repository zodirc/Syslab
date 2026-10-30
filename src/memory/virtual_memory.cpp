#include "syslab/memory/virtual_memory.hpp"

#include "syslab/core/check.hpp"
#include "syslab/core/error.hpp"

#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

#include <cstring>
#include <vector>

namespace syslab {

std::size_t system_page_size() {
  const long page = sysconf(_SC_PAGESIZE);
  if (page <= 0) {
    throw Error(Error::Code::Io, "sysconf(_SC_PAGESIZE) failed");
  }
  return static_cast<std::size_t>(page);
}

MappedRegion::MappedRegion(std::size_t bytes, bool readable, bool writable) {
  if (bytes == 0) {
    throw Error(Error::Code::InvalidArgument, "mapped region size must be non-zero");
  }
  const std::size_t page = system_page_size();
  const std::size_t mapped = align_up(bytes, page);
  int protection = PROT_NONE;
  if (readable) {
    protection |= PROT_READ;
  }
  if (writable) {
    protection |= PROT_WRITE;
  }
  void* pointer = mmap(nullptr, mapped, protection, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (pointer == MAP_FAILED) {
    throw Error(Error::Code::Io, "mmap failed");
  }
  data_ = pointer;
  size_ = mapped;
}

MappedRegion::~MappedRegion() { reset(); }

MappedRegion::MappedRegion(MappedRegion&& other) noexcept : data_(other.data_), size_(other.size_) {
  other.data_ = nullptr;
  other.size_ = 0;
}

MappedRegion& MappedRegion::operator=(MappedRegion&& other) noexcept {
  if (this != &other) {
    reset();
    data_ = other.data_;
    size_ = other.size_;
    other.data_ = nullptr;
    other.size_ = 0;
  }
  return *this;
}

void* MappedRegion::release() noexcept {
  void* pointer = data_;
  data_ = nullptr;
  size_ = 0;
  return pointer;
}

void MappedRegion::reset() noexcept {
  if (data_ != nullptr && size_ != 0) {
    munmap(data_, size_);
  }
  data_ = nullptr;
  size_ = 0;
}

void protect_region(void* data, std::size_t bytes, bool readable, bool writable) {
  int protection = PROT_NONE;
  if (readable) {
    protection |= PROT_READ;
  }
  if (writable) {
    protection |= PROT_WRITE;
  }
  if (mprotect(data, bytes, protection) != 0) {
    throw Error(Error::Code::Io, "mprotect failed");
  }
}

bool count_resident_pages(void* data, std::size_t bytes, std::size_t* resident_pages) {
  if (data == nullptr || bytes == 0 || resident_pages == nullptr) {
    return false;
  }
  const std::size_t page = system_page_size();
  const std::size_t pages = bytes / page;
  if (pages == 0 || bytes % page != 0) {
    return false;
  }
  std::vector<unsigned char> vector(pages);
  if (mincore(data, bytes, vector.data()) != 0) {
    return false;
  }
  std::size_t resident = 0;
  for (unsigned char entry : vector) {
    if ((entry & 1u) != 0) {
      ++resident;
    }
  }
  *resident_pages = resident;
  return true;
}

FaultCounters read_fault_counters() {
  rusage usage {};
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    throw Error(Error::Code::Io, "getrusage failed");
  }
  FaultCounters counters;
  counters.minor = usage.ru_minflt;
  counters.major = usage.ru_majflt;
  return counters;
}

}  // namespace syslab
