#pragma once

#include "syslab/core/json.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace syslab {

/**
 * @brief One ELF program header.
 * 一个 ELF 程序头。
 */
struct ElfSegment {
  std::uint32_t type = 0;
  std::uint32_t flags = 0;
  std::uint64_t offset = 0;
  std::uint64_t virtual_address = 0;
  std::uint64_t file_size = 0;
  std::uint64_t memory_size = 0;
};

/**
 * @brief One ELF section header.
 * 一个 ELF 节头。
 */
struct ElfSection {
  std::string name;
  std::uint32_t type = 0;
  std::uint64_t address = 0;
  std::uint64_t offset = 0;
  std::uint64_t size = 0;
};

/**
 * @brief One symbol-table entry.
 * 一条符号表记录。
 */
struct ElfSymbol {
  std::string name;
  std::uint64_t value = 0;
  std::uint64_t size = 0;
  unsigned char info = 0;
  std::uint16_t section_index = 0;
};

/**
 * @brief Teaching view of an ELF64 little-endian file.
 * ELF64 小端文件的教学视图。
 *
 * The reader covers the file header, program headers, section headers, and
 * the symbol tables it can name. It is not a general ELF toolkit. Other
 * classes, endians, and truncated files are reported through supported=false.
 * 读取器覆盖文件头、程序头、节头，以及它能命名的符号表。它不是通用 ELF 工具。其他类别、字节序和截断文件通过 supported=false 报告。
 */
struct ElfImage {
  bool supported = false;
  std::string limitation;
  std::uint16_t type = 0;
  std::uint16_t machine = 0;
  std::uint64_t entry = 0;
  bool has_dynamic = false;
  bool has_interpreter = false;
  std::vector<ElfSegment> segments;
  std::vector<ElfSection> sections;
  std::vector<ElfSymbol> symbols;
};

/**
 * @brief Reads ELF metadata from a file.
 * 从文件读取 ELF 元数据。
 * @param path Path to an ELF file.
 * ELF 文件路径。
 * @return Image. supported is false when the file is outside the teaching subset.
 * 映像。文件超出教学子集时 supported 为假。
 * @throws Error Io when the file cannot be read.
 * 文件无法读取时抛出 Error Io。
 */
ElfImage read_elf(const std::string& path);

/**
 * @brief Serializes an ELF image.
 * 序列化一份 ELF 映像。
 * @param image Parsed image.
 * 已解析的映像。
 * @return JSON object.
 * JSON 对象。
 */
Json elf_to_json(const ElfImage& image);

}  // namespace syslab
