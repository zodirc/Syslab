#pragma once

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

namespace syslab {

/**
 * @brief One field observed in a standard-layout type.
 * 在标准布局类型中观察到的一个字段。
 *
 * Offsets come from offsetof. For a type that is not standard layout, the
 * caller must not use this structure as a language-level guarantee.
 * 偏移来自 offsetof。类型不是标准布局时，调用者不得把这个结构当作语言级保证。
 */
struct FieldView {
  const char* name = "";
  std::size_t offset = 0;
  std::size_t size = 0;
};

/**
 * @brief Prints size, alignment, standard-layout status, and field offsets.
 * 打印大小、对齐、是否标准布局，以及字段偏移。
 *
 * The output labels the result as an ABI/compiler observation when the type
 * is not standard layout. offsetof is used only for standard-layout types.
 * 类型不是标准布局时，输出把结果标成 ABI 或编译器观察。offsetof 只用于标准布局类型。
 *
 * @tparam T Type being described.
 * 被描述的类型。
 * @param out Destination stream.
 * 目标流。
 * @param type_name Name printed for the type.
 * 打印出来的类型名。
 * @param fields Fields. Ignored when T is not standard layout.
 * 字段。T 不是标准布局时忽略。
 */
template <typename T>
void write_layout(std::ostream& out, const char* type_name, const std::vector<FieldView>& fields) {
  out << "type=" << type_name << " size=" << sizeof(T) << " align=" << alignof(T)
      << " standard_layout=" << (std::is_standard_layout<T>::value ? "true" : "false")
      << " polymorphic=" << (std::is_polymorphic<T>::value ? "true" : "false") << "\n";
  if (!std::is_standard_layout<T>::value) {
    out << "basis=ABI field offsets are not a C++ standard-layout guarantee\n";
    return;
  }
  for (const FieldView& field : fields) {
    out << "field name=" << field.name << " offset=" << field.offset << " size=" << field.size << "\n";
  }
}

}  // namespace syslab

/**
 * @brief Builds a FieldView with offsetof.
 * 用 offsetof 构造一个 FieldView。
 *
 * The type argument must be standard layout. The macro is the call site used
 * by layout experiments.
 * 类型参数必须是标准布局。这个宏是布局实验使用的调用点。
 */
#define SYSLAB_FIELD(Type, member) \
  ::syslab::FieldView { #member, offsetof(Type, member), sizeof(static_cast<Type*>(nullptr)->member) }
