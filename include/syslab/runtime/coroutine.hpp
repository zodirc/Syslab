#pragma once

#include "syslab/core/error.hpp"

#include <coroutine>
#include <exception>
#include <utility>
#include <vector>

namespace syslab {

class CoroutineScheduler;

/**
 * @brief Awaitable that returns the coroutine to the scheduler.
 * 把协程交回调度器的可等待对象。
 *
 * The coroutine remains suspended until the scheduler resumes it. At the
 * resume point the awaiter throws when the scheduler has been cancelled.
 * Cancellation is cooperative and is observed only at this awaiter.
 * 协程保持挂起，直到调度器恢复它。恢复点上，若调度器已取消，等待器抛出异常。取消是协作的，只在这个等待器上被观察到。
 */
struct YieldAwaiter {
  CoroutineScheduler* scheduler = nullptr;

  /**
   * @brief Always suspends.
   * 总是挂起。
   * @return False.
   * 假。
   */
  bool await_ready() const noexcept { return false; }

  /**
   * @brief Queues the coroutine and suspends it.
   * 把协程入队并挂起它。
   * @param handle Coroutine being suspended.
   * 正在挂起的协程。
   */
  void await_suspend(std::coroutine_handle<> handle) const;

  /**
   * @brief Continues the coroutine after the scheduler resumes it.
   * 调度器恢复之后继续该协程。
   * @throws Error Execution when the scheduler is cancelled.
   * 调度器已取消时抛出 Error Execution。
   */
  void await_resume() const;
};

/**
 * @brief Lazy task produced by a coroutine.
 * 协程产生的惰性任务。
 *
 * The task owns the coroutine frame. Moving the task transfers that ownership.
 * Destroying a task destroys the frame. A task does not run until a scheduler
 * resumes it. A coroutine is not a thread and does not run in parallel by itself.
 * 任务拥有协程帧。移动任务会转移该所有权。销毁任务会销毁帧。调度器恢复之前任务不运行。协程不是线程，本身也不会并行执行。
 */
class Task {
 public:
  struct promise_type {
    CoroutineScheduler* scheduler = nullptr;
    std::exception_ptr error;

    /**
     * @brief Builds the task that owns this promise.
     * 构造拥有该 promise 的任务。
     * @return Task bound to the coroutine frame.
     * 绑定到协程帧的任务。
     */
    Task get_return_object();

    /**
     * @brief Suspends before the body starts.
     * 在函数体开始前挂起。
     * @return Always-suspend awaiter.
     * 总是挂起的等待器。
     */
    std::suspend_always initial_suspend() noexcept { return {}; }

    /**
     * @brief Suspends at completion so the scheduler can observe the result.
     * 在完成时挂起，以便调度器观察结果。
     * @return Awaiter that leaves the frame alive.
     * 让帧保持存活的等待器。
     */
    std::suspend_always final_suspend() noexcept { return {}; }

    /**
     * @brief Completes a task that returns no value.
     * 完成一个没有返回值的任务。
     */
    void return_void() noexcept {}

    /**
     * @brief Stores an exception that escaped the coroutine body.
     * 保存从协程体逃出的异常。
     */
    void unhandled_exception() { error = std::current_exception(); }
  };

  Task() = default;
  explicit Task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
  Task& operator=(Task&& other) noexcept {
    if (this != &other) {
      reset();
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }
  ~Task() { reset(); }

  /**
   * @brief Returns the coroutine handle.
   * 返回协程句柄。
   * @return Handle, or null when the task is empty.
   * 句柄。任务为空时是空句柄。
   */
  std::coroutine_handle<promise_type> handle() const { return handle_; }

  /**
   * @brief Releases the handle without destroying the frame.
   * 释放句柄，不销毁帧。
   * @return Previous handle.
   * 原先的句柄。
   */
  std::coroutine_handle<promise_type> release() {
    std::coroutine_handle<promise_type> handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 private:
  void reset() {
    if (handle_) {
      handle_.destroy();
      handle_ = nullptr;
    }
  }
  std::coroutine_handle<promise_type> handle_{};
};

/**
 * @brief Single-threaded coroutine scheduler.
 * 单线程协程调度器。
 *
 * spawn queues a task. run resumes queued coroutines until the queue is empty.
 * yield_to returns the running coroutine to the queue. cancel makes the next
 * yield throw. Exceptions that escape a coroutine are rethrown from run.
 * spawn 把任务入队。run 恢复队列中的协程，直到队列为空。yield_to 把正在运行的协程放回队列。cancel 使下一次 yield 抛出异常。从协程逃出的异常由 run 重新抛出。
 */
class CoroutineScheduler {
 public:
  CoroutineScheduler() = default;
  CoroutineScheduler(const CoroutineScheduler&) = delete;
  CoroutineScheduler& operator=(const CoroutineScheduler&) = delete;
  ~CoroutineScheduler();

  /**
   * @brief Queues a task. The scheduler takes ownership of the frame.
   * 把任务入队。调度器取得帧的所有权。
   * @param task Task produced by a coroutine.
   * 协程产生的任务。
   */
  void spawn(Task task);

  /**
   * @brief Resumes queued coroutines until none remain ready.
   * 恢复已入队的协程，直到没有就绪协程。
   * @throws The first exception stored by a completed coroutine.
   * 已完成协程保存的第一个异常。
   */
  void run();

  /**
   * @brief Returns a coroutine to the ready queue.
   * 把协程放回就绪队列。
   * @param handle Coroutine to resume later.
   * 稍后要恢复的协程。
   */
  void enqueue(std::coroutine_handle<> handle);

  /**
   * @brief Requests cooperative cancellation.
   * 请求协作式取消。
   */
  void cancel() { cancelled_ = true; }

  /**
   * @brief Reports whether cancel has been called.
   * 报告是否已调用 cancel。
   * @return True after cancel.
   * 调用 cancel 之后为真。
   */
  bool cancelled() const { return cancelled_; }

 private:
  std::vector<std::coroutine_handle<Task::promise_type>> ready_;
  std::vector<std::coroutine_handle<Task::promise_type>> owned_;
  bool cancelled_ = false;
};

/**
 * @brief Suspends the current coroutine on a scheduler.
 * 把当前协程挂起到一个调度器上。
 * @param scheduler Scheduler that will resume it.
 * 将恢复它的调度器。
 * @return Awaiter bound to that scheduler.
 * 绑定到该调度器的等待器。
 */
inline YieldAwaiter yield_to(CoroutineScheduler& scheduler) { return YieldAwaiter{&scheduler}; }

}  // namespace syslab
