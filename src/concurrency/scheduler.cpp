#define _XOPEN_SOURCE 700

#include "syslab/concurrency/scheduler.hpp"

#include "syslab/core/error.hpp"
#include "syslab/memory/virtual_memory.hpp"

#include <ucontext.h>

#include <algorithm>
#include <thread>

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define SYSLAB_ASAN 1
#  endif
#  if __has_feature(thread_sanitizer)
#    define SYSLAB_TSAN 1
#  endif
#endif

#if defined(SYSLAB_ASAN)
extern "C" {
void __sanitizer_start_switch_fiber(void** fake_stack_save, const void* bottom, std::size_t size);
void __sanitizer_finish_switch_fiber(void* fake_stack_save, const void** bottom_old, std::size_t* size_old);
}
#endif

#if defined(SYSLAB_TSAN)
extern "C" {
void* __tsan_create_fiber(unsigned flags);
void __tsan_destroy_fiber(void* fiber);
void __tsan_switch_to_fiber(void* fiber, unsigned flags);
}
#endif

namespace syslab {
namespace {

thread_local CooperativeScheduler* g_scheduler = nullptr;

struct FiberContext {
  ucontext_t context {};
  ucontext_t* link = nullptr;
  MappedRegion stack;
  std::size_t guard = 0;
  void* tsan_fiber = nullptr;
};

}  // namespace

struct Fiber::Context : FiberContext {};

Fiber::Fiber(std::function<void()> body, std::size_t stack_size) : body_(std::move(body)) {
  if (!body_) {
    throw Error(Error::Code::InvalidArgument, "fiber body is empty");
  }
  const std::size_t page = system_page_size();
  if (stack_size < page * 2) {
    stack_size = page * 2;
  }
  stack_size = (stack_size + page - 1) & ~(page - 1);
  context_ = std::make_unique<Context>();
  context_->guard = page;
  context_->stack = MappedRegion(stack_size, true, true);
  protect_region(context_->stack.data(), page, false, false);
  if (getcontext(&context_->context) != 0) {
    throw Error(Error::Code::Io, "getcontext failed");
  }
  auto* base = static_cast<unsigned char*>(context_->stack.data());
  context_->context.uc_stack.ss_sp = base + page;
  context_->context.uc_stack.ss_size = context_->stack.size() - page;
  context_->context.uc_link = nullptr;
  const auto bits = reinterpret_cast<std::uintptr_t>(this);
  makecontext(&context_->context, reinterpret_cast<void (*)()>(entry), 2,
              static_cast<int>(bits >> 32), static_cast<int>(bits & 0xffffffffu));
#if defined(SYSLAB_TSAN)
  context_->tsan_fiber = __tsan_create_fiber(0);
#endif
}

Fiber::~Fiber() {
#if defined(SYSLAB_TSAN)
  if (context_ != nullptr && context_->tsan_fiber != nullptr) {
    __tsan_destroy_fiber(context_->tsan_fiber);
    context_->tsan_fiber = nullptr;
  }
#endif
}

void Fiber::entry(int high, int low) {
  const auto bits = (static_cast<std::uintptr_t>(static_cast<std::uint32_t>(high)) << 32) |
                    static_cast<std::uint32_t>(low);
  auto* fiber = reinterpret_cast<Fiber*>(bits);
#if defined(SYSLAB_ASAN)
  __sanitizer_finish_switch_fiber(nullptr, nullptr, nullptr);
#endif
  try {
    fiber->body_();
  } catch (...) {
    fiber->error_ = std::current_exception();
  }
  fiber->finished_ = true;
  fiber->suspend();
}

void switch_fiber(void** fake_stack, const void* bottom, std::size_t size, void* destination) {
#if defined(SYSLAB_TSAN)
  if (destination != nullptr) {
    __tsan_switch_to_fiber(destination, 0);
  }
#else
  (void)destination;
#endif
#if defined(SYSLAB_ASAN)
  __sanitizer_start_switch_fiber(fake_stack, bottom, size);
#else
  (void)fake_stack;
  (void)bottom;
  (void)size;
#endif
}

void finish_switch(void* fake_stack, void* returned_to) {
#if defined(SYSLAB_ASAN)
  __sanitizer_finish_switch_fiber(fake_stack, nullptr, nullptr);
#else
  (void)fake_stack;
#endif
#if defined(SYSLAB_TSAN)
  if (returned_to != nullptr) {
    __tsan_switch_to_fiber(returned_to, 0);
  }
#else
  (void)returned_to;
#endif
}

thread_local void* g_scheduler_fiber = nullptr;

void Fiber::resume(void* scheduler_context) {
  context_->link = static_cast<ucontext_t*>(scheduler_context);
  void* fake_stack = nullptr;
  const auto* base = static_cast<const unsigned char*>(context_->stack.data());
  switch_fiber(&fake_stack, base + context_->guard, context_->stack.size() - context_->guard, context_->tsan_fiber);
  if (swapcontext(context_->link, &context_->context) != 0) {
    throw Error(Error::Code::Io, "swapcontext into fiber failed");
  }
  finish_switch(fake_stack, g_scheduler_fiber);
}

void Fiber::suspend() {
  if (context_->link == nullptr) {
    throw Error(Error::Code::Logic, "fiber suspend without a scheduler context");
  }
  void* fake_stack = nullptr;
  switch_fiber(&fake_stack, nullptr, 0, g_scheduler_fiber);
  if (swapcontext(&context_->context, context_->link) != 0) {
    throw Error(Error::Code::Io, "swapcontext out of fiber failed");
  }
  finish_switch(fake_stack, context_->tsan_fiber);
}

void CooperativeScheduler::spawn(std::function<void()> body) {
  fibers_.push_back(std::make_unique<Fiber>(std::move(body)));
  ready_.push_back(fibers_.size() - 1);
}

CooperativeScheduler* CooperativeScheduler::current() { return g_scheduler; }

void CooperativeScheduler::yield() {
  if (g_scheduler == nullptr) {
    throw Error(Error::Code::Logic, "fiber yield outside a scheduler");
  }
  g_scheduler->yield_current();
}

void CooperativeScheduler::sleep_until(std::chrono::steady_clock::time_point deadline) {
  if (g_scheduler == nullptr) {
    throw Error(Error::Code::Logic, "fiber sleep outside a scheduler");
  }
  g_scheduler->park_current(deadline);
}

void CooperativeScheduler::yield_current() {
  ready_.push_back(current_);
  fibers_[current_]->suspend();
}

void CooperativeScheduler::park_current(std::chrono::steady_clock::time_point deadline) {
  sleeping_.push_back(Waiter{current_, deadline});
  fibers_[current_]->suspend();
}

void CooperativeScheduler::run() {
  if (running_) {
    throw Error(Error::Code::Logic, "scheduler is already running");
  }
  running_ = true;
  g_scheduler = this;
  ucontext_t loop {};
  loop_context_ = &loop;
#if defined(SYSLAB_TSAN)
  const bool created_scheduler_fiber = g_scheduler_fiber == nullptr;
  if (created_scheduler_fiber) {
    g_scheduler_fiber = __tsan_create_fiber(0);
  }
#endif
  try {
  while (true) {
    const auto now = std::chrono::steady_clock::now();
    for (auto it = sleeping_.begin(); it != sleeping_.end();) {
      if (it->deadline <= now) {
        ready_.push_back(it->index);
        it = sleeping_.erase(it);
      } else {
        ++it;
      }
    }
    if (ready_.empty()) {
      if (sleeping_.empty()) {
        break;
      }
      auto next = std::min_element(sleeping_.begin(), sleeping_.end(), [](const Waiter& left, const Waiter& right) {
        return left.deadline < right.deadline;
      });
      std::this_thread::sleep_until(next->deadline);
      continue;
    }
    current_ = ready_.front();
    ready_.erase(ready_.begin());
    Fiber& fiber = *fibers_[current_];
    if (!fiber.finished()) {
      fiber.resume(&loop);
    }
    if (fiber.finished() && fiber.error()) {
      std::rethrow_exception(fiber.error());
    }
  }
  } catch (...) {
    g_scheduler = nullptr;
    running_ = false;
    loop_context_ = nullptr;
#if defined(SYSLAB_TSAN)
    if (created_scheduler_fiber && g_scheduler_fiber != nullptr) {
      __tsan_destroy_fiber(g_scheduler_fiber);
      g_scheduler_fiber = nullptr;
    }
#endif
    throw;
  }
  g_scheduler = nullptr;
  running_ = false;
  loop_context_ = nullptr;
#if defined(SYSLAB_TSAN)
  if (created_scheduler_fiber && g_scheduler_fiber != nullptr) {
    __tsan_destroy_fiber(g_scheduler_fiber);
    g_scheduler_fiber = nullptr;
  }
#endif
}

}  // namespace syslab
