#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace syslab {

/**
 * @brief JSON value used for experiment records.
 * 实验记录使用的 JSON 值。
 *
 * Objects preserve insertion order. Numbers are stored as integers when the
 * token has no fraction and fits in a signed 64-bit value; otherwise they are
 * stored as decimal text so a digest or identifier is not rounded.
 * 对象保持插入顺序。数值在没有小数且能放进有符号 64 位时存为整数，否则存为十进制文本，避免摘要或标识被舍入。
 */
class Json {
 public:
  enum class Type { Null, Bool, Integer, Real, String, Array, Object };

  /**
   * @brief Creates a null value.
   * 创建一个 null 值。
   * @return Null JSON value.
   * JSON null。
   */
  static Json null();

  /**
   * @brief Creates a boolean value.
   * 创建一个布尔值。
   * @param value Boolean payload.
   * 布尔载荷。
   * @return JSON boolean.
   * JSON 布尔值。
   */
  static Json boolean(bool value);

  /**
   * @brief Creates an integer value.
   * 创建一个整数值。
   * @param value Signed integer payload.
   * 有符号整数载荷。
   * @return JSON number.
   * JSON 数字。
   */
  static Json integer(std::int64_t value);

  /**
   * @brief Creates a real value.
   * 创建一个实数值。
   * @param value Finite floating payload.
   * 有限浮点载荷。
   * @return JSON number.
   * JSON 数字。
   * @throws Error InvalidArgument when the value is not finite.
   * 值不是有限数时抛出 Error InvalidArgument。
   */
  static Json real(double value);

  /**
   * @brief Creates a string value.
   * 创建一个字符串值。
   * @param value UTF-8 text.
   * UTF-8 文本。
   * @return JSON string.
   * JSON 字符串。
   */
  static Json string(std::string value);

  /**
   * @brief Creates an empty array.
   * 创建一个空数组。
   * @return JSON array.
   * JSON 数组。
   */
  static Json array();

  /**
   * @brief Creates an empty object.
   * 创建一个空对象。
   * @return JSON object.
   * JSON 对象。
   */
  static Json object();

  /**
   * @brief Parses one JSON value.
   * 解析一个 JSON 值。
   * @param text JSON text. Leading and trailing whitespace is allowed.
   * JSON 文本。允许首尾空白。
   * @return Parsed value.
   * 解析得到的值。
   * @throws Error Parse when the text is not a single JSON value.
   * 文本不是单一 JSON 值时抛出 Error Parse。
   */
  static Json parse(const std::string& text);

  /**
   * @brief Appends an element to an array.
   * 向数组追加一个元素。
   * @param value Element to move into the array.
   * 移入数组的元素。
   * @return This array.
   * 这个数组。
   * @throws Error Logic when this value is not an array.
   * 当前值不是数组时抛出 Error Logic。
   */
  Json& push(Json value);

  /**
   * @brief Inserts or replaces an object field.
   * 插入或替换对象字段。
   * @param key Field name.
   * 字段名。
   * @param value Field value.
   * 字段值。
   * @return This object.
   * 这个对象。
   * @throws Error Logic when this value is not an object.
   * 当前值不是对象时抛出 Error Logic。
   */
  Json& set(const std::string& key, Json value);

  /**
   * @brief Serializes this value.
   * 序列化这个值。
   * @param indent Indent width. Zero selects compact output.
   * 缩进宽度。零表示紧凑输出。
   * @return JSON text.
   * JSON 文本。
   */
  std::string dump(int indent = 2) const;

  /**
   * @brief Returns the dynamic type.
   * 返回动态类型。
   * @return JSON type tag.
   * JSON 类型标记。
   */
  Type type() const noexcept { return type_; }

  /**
   * @brief Returns a boolean payload.
   * 返回布尔载荷。
   * @return Stored boolean.
   * 保存的布尔值。
   * @throws Error Logic when the type is not boolean.
   * 类型不是布尔时抛出 Error Logic。
   */
  bool as_bool() const;

  /**
   * @brief Returns an integer payload.
   * 返回整数载荷。
   * @return Stored integer.
   * 保存的整数。
   * @throws Error Logic when the type is not integer.
   * 类型不是整数时抛出 Error Logic。
   */
  std::int64_t as_integer() const;

  /**
   * @brief Returns a string payload.
   * 返回字符串载荷。
   * @return Stored string.
   * 保存的字符串。
   * @throws Error Logic when the type is not string.
   * 类型不是字符串时抛出 Error Logic。
   */
  const std::string& as_string() const;

  /**
   * @brief Returns array elements.
   * 返回数组元素。
   * @return Elements in order.
   * 按顺序排列的元素。
   * @throws Error Logic when the type is not an array.
   * 类型不是数组时抛出 Error Logic。
   */
  const std::vector<Json>& as_array() const;

  /**
   * @brief Finds an object field.
   * 查找对象字段。
   * @param key Field name.
   * 字段名。
   * @return Field value, or null when the field is absent or this is not an object.
   * 字段值。字段不存在或当前值不是对象时为空。
   */
  const Json* find(const std::string& key) const;

 private:
  Type type_ = Type::Null;
  bool bool_ = false;
  std::int64_t integer_ = 0;
  double real_ = 0;
  std::string text_;
  std::vector<Json> array_;
  std::vector<std::pair<std::string, Json>> object_;

  void dump_into(std::string* out, int indent, int depth) const;
};

}  // namespace syslab
