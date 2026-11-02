#include "experiments/common/check.hpp"

#include <cstring>

// 现象：键 k 第一次 add 4 得到 5。同一键再给参数 9，仍返回 5，执行计数不加。键 m 才会再次执行。
// 原理：键命中已有记录时直接返回第一次的结果，不按新参数重算。
namespace {

int g_calls = 0;

struct Record {
  const char* key;
  int result;
  bool used;
};

Record g_records[2];

int call_add(int value) {
  ++g_calls;
  return value + 1;
}

bool invoke(const char* key, int argument, int* result) {
  for (Record& record : g_records) {
    if (record.used && std::strcmp(record.key, key) == 0) {
      *result = record.result;
      return true;
    }
  }
  const int computed = call_add(argument);
  for (Record& record : g_records) {
    if (!record.used) {
      record.used = true;
      record.key = key;
      record.result = computed;
      *result = computed;
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  int first = 0;
  int repeated = 0;
  int other = 0;
  if (!invoke("k", 4, &first) || first != 5 || g_calls != 1) {
    syslab_step("键 k 的第一次调用没有得到 5。");
    return syslab_fail("first");
  }
  if (!invoke("k", 9, &repeated) || repeated != 5 || g_calls != 1) {
    syslab_step("同一个键用新参数又执行了一次。");
    return syslab_fail("repeat");
  }
  if (!invoke("m", 4, &other) || other != 5 || g_calls != 2) {
    syslab_step("另一个键没有执行。");
    return syslab_fail("other");
  }
  syslab_metric("calls", g_calls);
  syslab_note("LANG", "a repeated key returns the first result and does not run the tool again");
  syslab_step("键 k 第一次得到 5。同一键传入 9 仍得到 5，执行计数是 1。键 m 执行后计数是 2。");
  return syslab_pass();
}
