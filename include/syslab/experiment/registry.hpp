#pragma once

#include "syslab/experiment/manifest.hpp"

#include <map>
#include <string>
#include <vector>

namespace syslab {

/**
 * @brief Index of manifests discovered under an experiment tree.
 * 实验树下发现的清单索引。
 *
 * The registry owns manifest values. Identifiers must be unique. It does not
 * interpret experiment results.
 * 注册表拥有清单值。标识必须唯一。它不解释实验结果。
 */
class Registry {
 public:
  /**
   * @brief Loads every experiment.yaml below the root.
   * 加载根目录下的每一份 experiment.yaml。
   * @param experiments_root Directory that contains experiment families.
   * 包含实验族的目录。
   * @throws Error Validation when two manifests use one identifier.
   * 两份清单使用同一标识时抛出 Error Validation。
   * @throws Error Io or Parse or Validation from manifest loading.
   * 加载清单时的 Error Io、Parse 或 Validation。
   */
  void load_tree(const std::string& experiments_root);

  /**
   * @brief Returns identifiers in ascending order.
   * 按升序返回标识。
   * @return Experiment identifiers.
   * 实验标识。
   */
  std::vector<std::string> ids() const;

  /**
   * @brief Returns identifiers whose module equals the filter or starts with it.
   * 返回模块名等于过滤词或以它为前缀的标识。
   * @param module Module name or prefix. An empty filter returns every id.
   * 模块名或前缀。空过滤词返回全部标识。
   * @return Matching identifiers in ascending order.
   * 按升序排列的匹配标识。
   */
  std::vector<std::string> ids_for_module(const std::string& module) const;

  /**
   * @brief Returns a loaded manifest.
   * 返回已加载的清单。
   * @param id Experiment identifier.
   * 实验标识。
   * @return Manifest reference owned by the registry.
   * 注册表拥有的清单引用。
   * @throws Error NotFound when the identifier is absent.
   * 标识不存在时抛出 Error NotFound。
   */
  const Manifest& get(const std::string& id) const;

  /**
   * @brief Reports whether an identifier is loaded.
   * 报告标识是否已加载。
   * @param id Experiment identifier.
   * 实验标识。
   * @return True when the registry contains the identifier.
   * 注册表包含该标识时为真。
   */
  bool contains(const std::string& id) const;

 private:
  std::map<std::string, Manifest> manifests_;
};

}  // namespace syslab
