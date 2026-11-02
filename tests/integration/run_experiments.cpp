#include "syslab/experiment/registry.hpp"
#include "syslab/experiment/session.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <string>

namespace {

std::map<std::string, std::string> load_expectations(const std::string& path) {
  std::map<std::string, std::string> expectations;
  std::ifstream input(path);
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const std::size_t space = line.find(' ');
    if (space == std::string::npos || space == 0 || space + 1 >= line.size()) {
      continue;
    }
    expectations.emplace(line.substr(0, space), line.substr(space + 1));
  }
  return expectations;
}

}  // namespace

int main() {
#ifndef SYSLAB_SOURCE_DIR
  std::cerr << "SYSLAB_SOURCE_DIR is not defined\n";
  return 1;
#else
  const std::string root = SYSLAB_SOURCE_DIR;
  syslab::Registry registry;
  registry.load_tree(root + "/experiments");
  const std::map<std::string, std::string> expectations =
      load_expectations(root + "/tests/integration/expectations.txt");
  syslab::SessionConfig config;
  config.source_root = root;
  config.compiler = SYSLAB_COMPILER;
  config.library_dir = SYSLAB_LIBRARY_DIR;
  config.polyfill_dir = SYSLAB_POLYFILL_DIR;
  config.artifacts_dir = "/tmp/syslab-integration";
  config.use_cache = true;
  int failed = 0;
  for (const std::string& id : registry.ids()) {
    const auto found = expectations.find(id);
    const std::string expected = found == expectations.end() ? "PASSED" : found->second;
    const syslab::SessionOutput output = syslab::execute_session(registry.get(id), config);
    const std::string actual = syslab::to_string(output.state);
    if (actual != expected) {
      std::cerr << id << " expected " << expected << " actual " << actual << " " << output.message << "\n";
      ++failed;
    } else {
      std::cout << id << " " << actual << "\n";
    }
  }
  if (failed != 0) {
    std::cerr << failed << " experiment expectation(s) failed\n";
    return 1;
  }
  std::cout << "experiment expectations matched\n";
  return 0;
#endif
}
