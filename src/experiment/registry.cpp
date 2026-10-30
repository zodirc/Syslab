#include "syslab/experiment/registry.hpp"

#include "syslab/core/error.hpp"
#include "syslab/core/fs.hpp"

namespace syslab {

void Registry::load_tree(const std::string& experiments_root) {
  manifests_.clear();
  const std::vector<std::string> paths = find_files_named(experiments_root, "experiment.yaml");
  for (const std::string& path : paths) {
    Manifest manifest = load_manifest(path);
    const std::string id = manifest.experiment.id;
    if (manifests_.find(id) != manifests_.end()) {
      throw Error(Error::Code::Validation, "duplicate experiment id " + id);
    }
    manifests_.emplace(id, std::move(manifest));
  }
}

std::vector<std::string> Registry::ids() const {
  std::vector<std::string> identifiers;
  identifiers.reserve(manifests_.size());
  for (const auto& entry : manifests_) {
    identifiers.push_back(entry.first);
  }
  return identifiers;
}

std::vector<std::string> Registry::ids_for_module(const std::string& module) const {
  if (module.empty()) {
    return ids();
  }
  std::vector<std::string> identifiers;
  for (const auto& entry : manifests_) {
    const std::string& name = entry.second.experiment.module;
    if (name == module || (name.size() > module.size() && name.compare(0, module.size(), module) == 0 &&
                           name[module.size()] == '.')) {
      identifiers.push_back(entry.first);
    }
  }
  return identifiers;
}

const Manifest& Registry::get(const std::string& id) const {
  const auto found = manifests_.find(id);
  if (found == manifests_.end()) {
    throw Error(Error::Code::NotFound, "unknown experiment " + id);
  }
  return found->second;
}

bool Registry::contains(const std::string& id) const {
  return manifests_.find(id) != manifests_.end();
}

}  // namespace syslab
