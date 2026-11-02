#include "experiments/common/check.hpp"

#include <cstring>

// 现象：add 4 得到 5，执行计数是 1。add 100 不增加计数。未知名字 nope 也不增加计数。
// 原理：参数先按工具允许的范围检查。超出 0 到 10 时不调用函数。
namespace {

int g_calls = 0;

int add_one(int value) {
  ++g_calls;
  return value + 1;
}

bool dispatch(const char* name, int argument, int* result) {
  if (std::strcmp(name, "add") != 0) {
    return false;
  }
  if (argument < 0 || argument > 10) {
    return false;
  }
  *result = add_one(argument);
  return true;
}

}  // namespace

int main() {
  int result = 0;
  if (!dispatch("add", 4, &result) || result != 5 || g_calls != 1) {
    syslab_step("范围内的 add 4 没有返回 5。");
    return syslab_fail("in-range");
  }
  if (dispatch("add", 100, &result) || g_calls != 1) {
    syslab_step("超出范围的参数仍然调用了工具。");
    return syslab_fail("range");
  }
  if (dispatch("nope", 4, &result) || g_calls != 1) {
    syslab_step("未知名字调用了工具。");
    return syslab_fail("unknown");
  }
  syslab_metric("calls", g_calls);
  syslab_note("LANG", "arguments outside 0..10 are rejected before the tool function runs");
  syslab_step("add 4 得到 5，执行计数是 1。add 100 和 nope 都没有调用工具，计数仍是 1。");
  return syslab_pass();
}
