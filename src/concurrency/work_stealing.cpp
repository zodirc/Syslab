#include "syslab/concurrency/work_stealing.hpp"

#include "syslab/core/error.hpp"
#include "syslab/platform/cpu.hpp"

#include <atomic>
#include <exception>
#include <mutex>

namespace syslab {

void parallel_for(unsigned workers, std::size_t count, const std::function<void(std::size_t)>& body) {
  if (workers == 0) {
    throw Error(Error::Code::InvalidArgument, "work-stealing worker count must be non-zero");
  }
  if (count == 0 || !body) {
    return;
  }
  if (workers > count) {
    workers = static_cast<unsigned>(count);
  }
  constexpr std::size_t kCapacity = 4096;
  if ((count + workers - 1) / workers > kCapacity) {
    throw Error(Error::Code::ResourceLimit, "work-stealing initial slice exceeds the deque capacity");
  }
  std::vector<std::unique_ptr<ChaseLevDeque<kCapacity>>> deques;
  deques.reserve(workers);
  for (unsigned index = 0; index < workers; ++index) {
    deques.push_back(std::make_unique<ChaseLevDeque<kCapacity>>());
  }
  std::atomic<unsigned> ready{0};
  std::atomic<std::size_t> remaining{count};
  std::mutex error_mutex;
  std::exception_ptr error;
  std::vector<std::thread> threads;
  threads.reserve(workers);
  for (unsigned index = 0; index < workers; ++index) {
    threads.emplace_back([&, index]() {
      for (std::size_t item = index; item < count; item += workers) {
        if (!deques[index]->push(reinterpret_cast<void*>(item + 1))) {
          std::lock_guard<std::mutex> guard(error_mutex);
          if (!error) {
            error = std::make_exception_ptr(
                Error(Error::Code::ResourceLimit, "work-stealing push failed"));
          }
          remaining.store(0, std::memory_order_release);
          return;
        }
      }
      ready.fetch_add(1, std::memory_order_release);
      while (ready.load(std::memory_order_acquire) != workers) {
        cpu_relax();
      }
      while (remaining.load(std::memory_order_acquire) != 0) {
        void* claimed = deques[index]->pop();
        if (claimed == nullptr) {
          for (unsigned other = 0; other < workers && claimed == nullptr; ++other) {
            if (other != index) {
              claimed = deques[other]->steal();
            }
          }
        }
        if (claimed == nullptr) {
          cpu_relax();
          continue;
        }
        const auto item = reinterpret_cast<std::uintptr_t>(claimed) - 1;
        try {
          body(static_cast<std::size_t>(item));
        } catch (...) {
          std::lock_guard<std::mutex> guard(error_mutex);
          if (!error) {
            error = std::current_exception();
          }
        }
        remaining.fetch_sub(1, std::memory_order_acq_rel);
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  if (error) {
    std::rethrow_exception(error);
  }
}

}  // namespace syslab
