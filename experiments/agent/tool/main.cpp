#include "experiments/common/check.hpp"

#include <cstring>

// 现象：调用 add 得到 5，并且执行计数加 1。调用未知名字 nope 得到 not_found，执行计数不变。
// 原理：工具按名字分派。名字不在表里时不调用任何工具函数。
namespace {

int g_calls = 0;

int add_one(int value) {
  ++g_calls;
  return value + 1;
}

struct Tool {
  const char* name;
  int (*function)(int);
};

const Tool kTools[] = {{"add", add_one}};

bool dispatch(const char* name, int argument, int* result) {
  for (const Tool& tool : kTools) {
    if (std::strcmp(tool.name, name) == 0) {
      *result = tool.function(argument);
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  int result = 0;
  if (!dispatch("add", 4, &result) || result != 5 || g_calls != 1) {
    syslab_step("已知工具 add 没有返回 5。");
    return syslab_fail("known");
  }
  if (dispatch("nope", 4, &result) || g_calls != 1) {
    syslab_step("未知工具 nope 被执行了。");
    return syslab_fail("unknown");
  }
  syslab_metric("calls", g_calls);
  syslab_note("LANG", "dispatch runs a tool only when its name is present in the table");
  syslab_step("add 4 得到 5，执行计数是 1。nope 没有被执行，执行计数仍是 1。");
  return syslab_pass();
}
