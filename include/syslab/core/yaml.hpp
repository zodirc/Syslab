#pragma once

#include <string>
#include <utility>
#include <vector>

namespace syslab {

/**
 * @brief YAML subset node used by experiment manifests.
 * 实验清单使用的 YAML 子集节点。
 *
 * The parser accepts mappings, sequences, scalars, comments, and the empty
 * inline sequence `[]`. Indentation must use spaces and increase by two.
 * Tabs, anchors, tags, multiline scalars, and flow mappings are rejected.
 * 解析器接受映射、序列、标量、注释，以及空的行内序列 `[]`。缩进必须是空格，并且每次增加两个。制表符、锚点、标签、多行标量和流式映射会被拒绝。
 */
class Yaml {
 public:
  enum class Kind { Null, Scalar, Sequence, Map };

  /**
   * @brief Parses a YAML document in the supported subset.
   * 解析受支持子集中的一份 YAML 文档。
   * @param text Manifest text.
   * 清单文本。
   * @param origin Path or label used in parse errors.
   * 解析错误里使用的路径或标签。
   * @return Root node.
   * 根节点。
   * @throws Error Parse when the text leaves the supported subset.
   * 文本超出受支持子集时抛出 Error Parse。
   */
  static Yaml parse(const std::string& text, const std::string& origin);

  /**
   * @brief Returns the node kind.
   * 返回节点种类。
   * @return Kind tag.
   * 种类标记。
   */
  Kind kind() const noexcept { return kind_; }

  /**
   * @brief Returns a scalar as text.
   * 把标量作为文本返回。
   * @return Scalar characters without surrounding quotes.
   * 去掉两侧引号后的标量字符。
   * @throws Error Validation when the node is not a scalar.
   * 节点不是标量时抛出 Error Validation。
   */
  const std::string& as_scalar() const;

  /**
   * @brief Parses a scalar as a signed integer.
   * 把标量解析为有符号整数。
   * @return Integer value.
   * 整数值。
   * @throws Error Validation when the scalar is not an integer.
   * 标量不是整数时抛出 Error Validation。
   */
  long long as_integer() const;

  /**
   * @brief Parses a scalar as a boolean.
   * 把标量解析为布尔值。
   * @return Boolean value. Accepts true and false only.
   * 布尔值。只接受 true 和 false。
   * @throws Error Validation when the scalar is not a boolean.
   * 标量不是布尔值时抛出 Error Validation。
   */
  bool as_bool() const;

  /**
   * @brief Returns sequence elements.
   * 返回序列元素。
   * @return Elements in document order.
   * 文档顺序中的元素。
   * @throws Error Validation when the node is not a sequence.
   * 节点不是序列时抛出 Error Validation。
   */
  const std::vector<Yaml>& as_sequence() const;

  /**
   * @brief Finds a mapping entry.
   * 查找映射项。
   * @param key Entry name.
   * 项的名称。
   * @return Entry node, or null when the key is absent.
   * 项节点。键不存在时为空。
   */
  const Yaml* find(const std::string& key) const;

  /**
   * @brief Returns a required mapping entry.
   * 返回必须存在的映射项。
   * @param key Entry name.
   * 项的名称。
   * @return Entry node.
   * 项节点。
   * @throws Error Validation when the key is absent.
   * 键不存在时抛出 Error Validation。
   */
  const Yaml& require(const std::string& key) const;

  /**
   * @brief Returns mapping entries in document order.
   * 按文档顺序返回映射项。
   * @return Key and value pairs.
   * 键和值的对。
   * @throws Error Validation when the node is not a mapping.
   * 节点不是映射时抛出 Error Validation。
   */
  const std::vector<std::pair<std::string, Yaml>>& items() const;

 private:
  Kind kind_ = Kind::Null;
  std::string scalar_;
  std::vector<Yaml> sequence_;
  std::vector<std::pair<std::string, Yaml>> map_;

  friend struct YamlParser;
};

}  // namespace syslab
