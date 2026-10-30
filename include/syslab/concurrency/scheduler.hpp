#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace syslab {

/**
 * @brief Cooperative fiber with a private stack.
 * 带私有栈的协作式 fiber。
 *
 * resume switches to the fiber. yield returns to the scheduler. The fiber
 * function must not throw across the context switch; the scheduler catches
 * exceptions at the fiber entry and records them. The stack has a guard page
 * at the low address. This uses POSIX ucontext and is a teaching context
 * switch, not a production runtime. It must not be called from a signal handler.
 * resume 切换到 fiber。yield 回到调度器。fiber 函数不得把异常抛过上下文切换；调度器在 fiber 入口捕获异常并记录。栈的低地址有一页保护页。这里使用 POSIX ucontext，是教学上下文切换，不是生产运行时。不得从信号处理函数调用。
 */
class Fiber {
 public:
  /**
   * @brief Creates a fiber.
   * 创建一个 fiber。
   * @param body Function run on the fiber stack.
   * 在 fiber 栈上运行的函数。
   * @param stack_size Stack bytes, rounded up to at least two pages.
   * 栈字节数，至少向上取整到两页。
   */
  explicit Fiber(std::function<void()> body, std::size_t stack_size = 64 * 1024);
  ~Fiber();
  Fiber(const Fiber&) = delete;
  Fiber& operator=(const Fiber&) = delete;

  /**
   * @brief Switches from the scheduler context to this fiber.
   * 从调度器上下文切换到这个 fiber。
   * @param scheduler_context Context that resume returns to. Opaque to callers outside the scheduler.
   * resume 返回到的上下文。对调度器之外的调用者是不透明的。
   */
  void resume(void* scheduler_context);

  /**
   * @brief Returns to the scheduler context.
   * 回到调度器上下文。
   */
  void suspend();

  /**
   * @brief Reports whether the body has returned.
   * 报告函数体是否已经返回。
   * @return True after the body returns or throws.
   * 函数体返回或抛出后为真。
   */
  bool finished() const noexcept { return finished_; }

  /**
   * @brief Returns the exception thrown by the body, if any.
   * 返回函数体抛出的异常，如果有的话。
   * @return Exception pointer, or null when the body did not throw.
   * 异常指针。函数体没有抛出时为空。
   */
  std::exception_ptr error() const { return error_; }

 private:
  friend class CooperativeScheduler;
  static void entry(int high, int low);
  struct Context;
  std::unique_ptr<Context> context_;
  std::function<void()> body_;
  bool finished_ = false;
  std::exception_ptr error_;
};

/**
 * @brief Cooperative scheduler for fibers.
 * fiber 的协作式调度器。
 *
 * Fibers run one at a time on the calling thread. yield moves the current
 * fiber to the end of the ready queue. sleep_for parks it until the deadline
 * and the scheduler has no earlier ready fiber. This does not create kernel
 * threads and does not preempt a fiber that never yields.
 * fiber 在调用线程上一次运行一个。yield 把当前 fiber 放到就绪队列末尾。sleep_for 把它停到截止时间，并且调度器没有更早的就绪 fiber。这不创建内核线程，也不抢占从不让出的 fiber。
 */
class CooperativeScheduler {
 public:
  CooperativeScheduler() = default;
  CooperativeScheduler(const CooperativeScheduler&) = delete;
  CooperativeScheduler& operator=(const CooperativeScheduler&) = delete;

  /**
   * @brief Queues a new fiber.
   * 把一个新 fiber 放入队列。
   * @param body Fiber function.
   * fiber 函数。
   */
  void spawn(std::function<void()> body);

  /**
   * @brief Runs until every fiber has finished.
   * 一直运行到每个 fiber 结束。
   * @throws Error Execution when a fiber body threw. The exception is rethrown.
   * fiber 函数体抛出异常时为 Error Execution。该异常会被重新抛出。
   */
  void run();

  /**
   * @brief Yields the running fiber.
   * 让正在运行的 fiber 让出。
   */
  static void yield();

  /**
   * @brief Parks the running fiber until the deadline.
   * 把正在运行的 fiber 停到截止时间。
   * @param deadline Time point on the steady clock.
   * steady_clock 上的时间点。
   */
  static void sleep_until(std::chrono::steady_clock::time_point deadline);

  /**
   * @brief Returns the scheduler running on this thread.
   * 返回本线程正在运行的调度器。
   * @return Active scheduler, or null outside run.
   * 活动的调度器。不在 run 之中时为空。
   */
  static CooperativeScheduler* current();

 private:
  friend class Fiber;
  struct Waiter {
    std::size_t index = 0;
    std::chrono::steady_clock::time_point deadline;
  };
  void yield_current();
  void park_current(std::chrono::steady_clock::time_point deadline);
  std::vector<std::unique_ptr<Fiber>> fibers_;
  std::vector<std::size_t> ready_;
  std::vector<Waiter> sleeping_;
  std::size_t current_ = static_cast<std::size_t>(-1);
  void* loop_context_ = nullptr;
  bool running_ = false;
};

}  // namespace syslab
