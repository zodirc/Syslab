#include "experiments/common/check.hpp"
#include <fstream>
#include <string>
// 应用：准备比较跨 NUMA 节点的内存延迟。
// 原理：sysfs 中可见的节点目录少于两个时，主机不能做跨节点比较，结果为不支持。节点不少于两个时，本节只记录节点数量，不绑定内存，也不比较延迟。
int main() {
  int nodes = 0;
  for (int index = 0; index < 64; ++index) {
    const std::string path = "/sys/devices/system/node/node" + std::to_string(index);
    std::ifstream input(path + "/cpulist");
    if (input) ++nodes;
  }
  syslab_metric("numa_nodes", nodes);
  if (nodes < 2) {
    syslab_step("可见 NUMA 节点少于两个，本次不比较跨节点延迟。");
    return syslab_unsupported("fewer-than-two-numa-nodes");
  }
  syslab_note("OS", "two node directories are visible; this run does not bind memory and does not assert a latency delta");
    syslab_step("可见 NUMA 节点不少于两个。本次只记录节点数，不比较延迟。");
  return syslab_pass();
}
