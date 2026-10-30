#include "experiments/common/check.hpp"
#include <csignal>
volatile sig_atomic_t delivered = 0;
void handler(int) { delivered = 1; }
// 应用：已经调用 raise 或 kill，处理函数没有执行。
// 原理：sigaction 安装 SIGUSR1 的处理函数后，raise 使该函数执行一次，delivered 变为 1。处理函数仍然受异步信号安全约束。
int main() {
  struct sigaction action {};
  action.sa_handler = handler;
  if (sigaction(SIGUSR1, &action, nullptr) != 0) {
    syslab_step("信号处理函数没有安装。");
    return syslab_fail("sigaction");
  }
  if (raise(SIGUSR1) != 0) {
    syslab_step("信号没有发出。");
    return syslab_fail("raise");
  }
  if (delivered != 1) {
    syslab_step("处理函数没有执行。");
    return syslab_fail("delivery");
  }
  syslab_note("OS", "the handler ran for SIGUSR1; asynchronous handler restrictions still apply");
    syslab_step("SIGUSR1 的处理函数执行了一次，delivered 为 1。");
  return syslab_pass();
}
