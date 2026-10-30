#include "syslab/memory/thread_cache_allocator.hpp"

#include "internal.hpp"

#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace syslab {
namespace {

constexpr std::uint32_t kCached = 0xCAC4E000u;
constexpr int kBinLimit = 64;
constexpr int kRefill = 16;

int current_tid() { return static_cast<int>(syscall(SYS_gettid)); }

struct Prefix {
  std::uint32_t magic;
  std::uint32_t tid;
  std::uint32_t user_size;
  std::uint32_t alignment;
  void* upstream;
  std::uint32_t upstream_size;
  std::uint32_t upstream_alignment;
};

std::size_t class_for(std::size_t size) {
  std::size_t klass = 16;
  while (klass < size) {
    if (klass > (static_cast<std::size_t>(1) << (sizeof(std::size_t) * 8 - 2))) {
      return 0;
    }
    klass *= 2;
  }
  return klass;
}

}  // namespace

struct ThreadCacheAllocator::State {
  std::mutex mutex;
  std::unique_ptr<IAllocator> upstream;
  std::atomic<std::size_t> allocated_bytes {0};
  std::atomic<std::size_t> allocation_count {0};
  std::atomic<std::size_t> deallocation_count {0};
  std::atomic<std::size_t> peak_allocated {0};
  std::atomic<std::size_t> cached_bytes {0};
};

struct ThreadCacheAccess {
  struct Cache {
  std::shared_ptr<ThreadCacheAllocator::State> state;
  std::unordered_map<std::size_t, std::vector<void*>> bins;

  ~Cache() { flush(); }

  void flush() {
    if (!state || !state->upstream) {
      return;
    }
    std::lock_guard<std::mutex> guard(state->mutex);
    for (auto& entry : bins) {
      for (void* pointer : entry.second) {
        auto* prefix = reinterpret_cast<Prefix*>(static_cast<unsigned char*>(pointer) - sizeof(Prefix));
        if (prefix->magic == kCached) {
          state->cached_bytes.fetch_sub(prefix->user_size, std::memory_order_relaxed);
          state->upstream->deallocate(prefix->upstream, prefix->upstream_size, prefix->upstream_alignment);
        }
      }
      entry.second.clear();
    }
  }
};

static Cache& local_cache(const std::shared_ptr<ThreadCacheAllocator::State>& state) {
  thread_local std::unordered_map<ThreadCacheAllocator::State*, Cache> caches;
  Cache& cache = caches[state.get()];
  if (!cache.state) {
    cache.state = state;
  }
  return cache;
}

static void note_peak(ThreadCacheAllocator::State* state, std::size_t allocated) {
  std::size_t peak = state->peak_allocated.load(std::memory_order_relaxed);
  while (allocated > peak &&
         !state->peak_allocated.compare_exchange_weak(peak, allocated, std::memory_order_relaxed)) {
  }
}

};

ThreadCacheAllocator::ThreadCacheAllocator(std::unique_ptr<IAllocator> upstream) {
  if (!upstream) {
    throw Error(Error::Code::InvalidArgument, "thread-cache upstream allocator is null");
  }
  state_ = std::make_shared<State>();
  state_->upstream = std::move(upstream);
}

ThreadCacheAllocator::~ThreadCacheAllocator() = default;

void* ThreadCacheAllocator::allocate(const AllocationRequest& request) {
  mem::validate_request(request);
  if (request.size > std::numeric_limits<std::uint32_t>::max() ||
      request.alignment > std::numeric_limits<std::uint32_t>::max()) {
    return nullptr;
  }
  const std::size_t klass = class_for(request.size);
  const bool cacheable = klass != 0 && klass <= 4096;
  ThreadCacheAccess::Cache& cache = ThreadCacheAccess::local_cache(state_);
  if (cacheable) {
    std::vector<void*>& bin = cache.bins[request.size];
    if (!bin.empty()) {
      void* pointer = bin.back();
      bin.pop_back();
      auto* prefix = reinterpret_cast<Prefix*>(static_cast<unsigned char*>(pointer) - sizeof(Prefix));
      if (prefix->magic != kCached || prefix->user_size != request.size ||
          prefix->alignment != request.alignment) {
        throw Error(Error::Code::Corruption, "thread-cache bin contains a damaged block");
      }
      prefix->magic = mem::kLive;
      prefix->tid = static_cast<std::uint32_t>(current_tid());
      prefix->alignment = static_cast<std::uint32_t>(request.alignment);
      state_->cached_bytes.fetch_sub(prefix->user_size, std::memory_order_relaxed);
      const std::size_t allocated = state_->allocated_bytes.fetch_add(request.size, std::memory_order_relaxed) + request.size;
      state_->allocation_count.fetch_add(1, std::memory_order_relaxed);
      ThreadCacheAccess::note_peak(state_.get(), allocated);
      return pointer;
    }
  }
  const std::size_t alignment = std::max(request.alignment, alignof(Prefix));
  const std::size_t prefix_bytes = align_up(sizeof(Prefix), alignment);
  std::size_t total = 0;
  if (add_overflow(prefix_bytes, request.size, &total)) {
    return nullptr;
  }
  void* upstream_pointer = nullptr;
  {
    std::lock_guard<std::mutex> guard(state_->mutex);
    upstream_pointer = state_->upstream->allocate(AllocationRequest{total, alignment, request.tag});
  }
  if (upstream_pointer == nullptr) {
    return nullptr;
  }
  unsigned char* user = static_cast<unsigned char*>(upstream_pointer) + prefix_bytes;
  auto* prefix = reinterpret_cast<Prefix*>(user - sizeof(Prefix));
  prefix->magic = mem::kLive;
  prefix->tid = static_cast<std::uint32_t>(current_tid());
  prefix->user_size = static_cast<std::uint32_t>(request.size);
  prefix->alignment = static_cast<std::uint32_t>(request.alignment);
  prefix->upstream = upstream_pointer;
  prefix->upstream_size = static_cast<std::uint32_t>(total);
  prefix->upstream_alignment = static_cast<std::uint32_t>(alignment);
  if (cacheable) {
    std::lock_guard<std::mutex> guard(state_->mutex);
    for (int extra = 1; extra < kRefill; ++extra) {
      void* refill = state_->upstream->allocate(AllocationRequest{total, alignment, request.tag});
      if (refill == nullptr) {
        break;
      }
      unsigned char* refill_user = static_cast<unsigned char*>(refill) + prefix_bytes;
      auto* refill_prefix = reinterpret_cast<Prefix*>(refill_user - sizeof(Prefix));
      refill_prefix->magic = kCached;
      refill_prefix->tid = prefix->tid;
      refill_prefix->user_size = prefix->user_size;
      refill_prefix->alignment = prefix->alignment;
      refill_prefix->upstream = refill;
      refill_prefix->upstream_size = prefix->upstream_size;
      refill_prefix->upstream_alignment = prefix->upstream_alignment;
      cache.bins[request.size].push_back(refill_user);
      state_->cached_bytes.fetch_add(request.size, std::memory_order_relaxed);
    }
  }
  const std::size_t allocated = state_->allocated_bytes.fetch_add(request.size, std::memory_order_relaxed) + request.size;
  state_->allocation_count.fetch_add(1, std::memory_order_relaxed);
  ThreadCacheAccess::note_peak(state_.get(), allocated);
  return user;
}

void ThreadCacheAllocator::deallocate(void* pointer, std::size_t size, std::size_t alignment) {
  if (pointer == nullptr) {
    return;
  }
  auto* prefix = reinterpret_cast<Prefix*>(static_cast<unsigned char*>(pointer) - sizeof(Prefix));
  if ((prefix->magic != mem::kLive && prefix->magic != kCached) || prefix->user_size != size ||
      prefix->alignment != alignment) {
    throw Error(Error::Code::Corruption, "thread-cache pointer does not match the free request");
  }
  if (prefix->magic == kCached) {
    throw Error(Error::Code::Corruption, "thread-cache double free");
  }
  const std::size_t klass = class_for(size);
  if (klass != 0 && klass <= 4096 && prefix->tid == static_cast<std::uint32_t>(current_tid())) {
    ThreadCacheAccess::Cache& cache = ThreadCacheAccess::local_cache(state_);
    std::vector<void*>& bin = cache.bins[size];
    for (void* existing : bin) {
      if (existing == pointer) {
        throw Error(Error::Code::Corruption, "thread-cache double free");
      }
    }
    prefix->magic = kCached;
    bin.push_back(pointer);
    state_->cached_bytes.fetch_add(size, std::memory_order_relaxed);
    state_->allocated_bytes.fetch_sub(size, std::memory_order_relaxed);
    state_->deallocation_count.fetch_add(1, std::memory_order_relaxed);
    if (static_cast<int>(bin.size()) > kBinLimit) {
      std::lock_guard<std::mutex> guard(state_->mutex);
      const std::size_t keep = bin.size() / 2;
      while (bin.size() > keep) {
        void* extra = bin.back();
        bin.pop_back();
        auto* extra_prefix = reinterpret_cast<Prefix*>(static_cast<unsigned char*>(extra) - sizeof(Prefix));
        state_->cached_bytes.fetch_sub(extra_prefix->user_size, std::memory_order_relaxed);
        state_->upstream->deallocate(extra_prefix->upstream, extra_prefix->upstream_size,
                                     extra_prefix->upstream_alignment);
      }
    }
    return;
  }
  {
    std::lock_guard<std::mutex> guard(state_->mutex);
    state_->upstream->deallocate(prefix->upstream, prefix->upstream_size, prefix->upstream_alignment);
  }
  state_->allocated_bytes.fetch_sub(size, std::memory_order_relaxed);
  state_->deallocation_count.fetch_add(1, std::memory_order_relaxed);
}

AllocationStats ThreadCacheAllocator::statistics() const {
  AllocationStats stats;
  stats.allocated_bytes = state_->allocated_bytes.load(std::memory_order_relaxed);
  stats.allocation_count = state_->allocation_count.load(std::memory_order_relaxed);
  stats.deallocation_count = state_->deallocation_count.load(std::memory_order_relaxed);
  stats.peak_allocated_bytes = state_->peak_allocated.load(std::memory_order_relaxed);
  stats.free_bytes = state_->cached_bytes.load(std::memory_order_relaxed);
  stats.available_fields = static_cast<std::uint32_t>(StatField::Allocated) |
                           static_cast<std::uint32_t>(StatField::Free) |
                           static_cast<std::uint32_t>(StatField::AllocationCount) |
                           static_cast<std::uint32_t>(StatField::DeallocationCount) |
                           static_cast<std::uint32_t>(StatField::PeakAllocated);
  std::lock_guard<std::mutex> guard(state_->mutex);
  const AllocationStats upstream = state_->upstream->statistics();
  if (upstream.has(StatField::Reserved)) {
    stats.reserved_bytes = upstream.reserved_bytes;
    stats.available_fields |= static_cast<std::uint32_t>(StatField::Reserved);
  }
  return stats;
}

}  // namespace syslab
