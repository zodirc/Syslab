#include "syslab/runtime/coroutine.hpp"

namespace syslab {

Task Task::promise_type::get_return_object() {
  return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
}

void YieldAwaiter::await_suspend(std::coroutine_handle<> handle) const { scheduler->enqueue(handle); }

void YieldAwaiter::await_resume() const {
  if (scheduler != nullptr && scheduler->cancelled()) {
    throw Error(Error::Code::Execution, "coroutine cancelled");
  }
}

CoroutineScheduler::~CoroutineScheduler() {
  for (std::coroutine_handle<Task::promise_type> handle : owned_) {
    if (handle) {
      handle.destroy();
    }
  }
}

void CoroutineScheduler::spawn(Task task) {
  std::coroutine_handle<Task::promise_type> handle = task.release();
  if (!handle) {
    throw Error(Error::Code::InvalidArgument, "cannot spawn an empty task");
  }
  handle.promise().scheduler = this;
  owned_.push_back(handle);
  ready_.push_back(handle);
}

void CoroutineScheduler::enqueue(std::coroutine_handle<> handle) {
  ready_.push_back(std::coroutine_handle<Task::promise_type>::from_address(handle.address()));
}

void CoroutineScheduler::run() {
  std::exception_ptr error;
  while (!ready_.empty()) {
    std::coroutine_handle<Task::promise_type> handle = ready_.front();
    ready_.erase(ready_.begin());
    handle.resume();
    if (handle.done() && handle.promise().error) {
      error = handle.promise().error;
      ready_.clear();
    }
  }
  if (error) {
    std::rethrow_exception(error);
  }
}

}  // namespace syslab
