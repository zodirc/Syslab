#include "syslab/observation/elf.hpp"

#include "syslab/core/error.hpp"
#include "syslab/core/fs.hpp"

#include <cstring>

namespace syslab {
namespace {

constexpr unsigned char kMagic[4] = {0x7f, 'E', 'L', 'F'};
constexpr int kElf64 = 2;
constexpr int kLittle = 1;
constexpr std::uint32_t kPtDynamic = 2;
constexpr std::uint32_t kPtInterp = 3;
constexpr std::uint32_t kShtSymtab = 2;
constexpr std::uint32_t kShtDynsym = 11;

template <typename T>
T read_pod(const std::string& bytes, std::size_t offset) {
  if (offset + sizeof(T) > bytes.size()) {
    throw Error(Error::Code::Parse, "ELF field extends past the file");
  }
  T value {};
  std::memcpy(&value, bytes.data() + offset, sizeof(T));
  return value;
}

std::string section_name(const std::string& bytes, std::uint64_t table_offset, std::uint64_t table_size,
                         std::uint32_t name_offset) {
  if (name_offset >= table_size || table_offset + table_size > bytes.size()) {
    return {};
  }
  const std::size_t start = static_cast<std::size_t>(table_offset + name_offset);
  const std::size_t end = bytes.find('\0', start);
  if (end == std::string::npos || end > table_offset + table_size) {
    return {};
  }
  return bytes.substr(start, end - start);
}

}  // namespace

ElfImage read_elf(const std::string& path) {
  const std::string bytes = read_file(path);
  ElfImage image;
  if (bytes.size() < 64 || std::memcmp(bytes.data(), kMagic, 4) != 0) {
    image.limitation = "file is not an ELF object";
    return image;
  }
  if (static_cast<unsigned char>(bytes[4]) != kElf64 || static_cast<unsigned char>(bytes[5]) != kLittle) {
    image.limitation = "only ELF64 little-endian files are read";
    return image;
  }
  image.type = read_pod<std::uint16_t>(bytes, 16);
  image.machine = read_pod<std::uint16_t>(bytes, 18);
  image.entry = read_pod<std::uint64_t>(bytes, 24);
  const auto program_offset = read_pod<std::uint64_t>(bytes, 32);
  const auto section_offset = read_pod<std::uint64_t>(bytes, 40);
  const auto program_size = read_pod<std::uint16_t>(bytes, 54);
  const auto program_count = read_pod<std::uint16_t>(bytes, 56);
  const auto section_size = read_pod<std::uint16_t>(bytes, 58);
  const auto section_count = read_pod<std::uint16_t>(bytes, 60);
  const auto name_index = read_pod<std::uint16_t>(bytes, 62);
  if (program_size < 56 || section_size < 64) {
    image.limitation = "ELF header sizes are smaller than ELF64";
    return image;
  }
  for (int index = 0; index < program_count; ++index) {
    const std::size_t offset = static_cast<std::size_t>(program_offset) + static_cast<std::size_t>(index) * program_size;
    ElfSegment segment;
    segment.type = read_pod<std::uint32_t>(bytes, offset);
    segment.flags = read_pod<std::uint32_t>(bytes, offset + 4);
    segment.offset = read_pod<std::uint64_t>(bytes, offset + 8);
    segment.virtual_address = read_pod<std::uint64_t>(bytes, offset + 16);
    segment.file_size = read_pod<std::uint64_t>(bytes, offset + 32);
    segment.memory_size = read_pod<std::uint64_t>(bytes, offset + 40);
    if (segment.type == kPtDynamic) {
      image.has_dynamic = true;
    }
    if (segment.type == kPtInterp) {
      image.has_interpreter = true;
    }
    image.segments.push_back(segment);
  }
  std::uint64_t name_offset = 0;
  std::uint64_t name_size = 0;
  if (name_index < section_count) {
    const std::size_t offset =
        static_cast<std::size_t>(section_offset) + static_cast<std::size_t>(name_index) * section_size;
    name_offset = read_pod<std::uint64_t>(bytes, offset + 24);
    name_size = read_pod<std::uint64_t>(bytes, offset + 32);
  }
  for (int index = 0; index < section_count; ++index) {
    const std::size_t offset = static_cast<std::size_t>(section_offset) + static_cast<std::size_t>(index) * section_size;
    ElfSection section;
    const auto name = read_pod<std::uint32_t>(bytes, offset);
    section.type = read_pod<std::uint32_t>(bytes, offset + 4);
    section.address = read_pod<std::uint64_t>(bytes, offset + 16);
    section.offset = read_pod<std::uint64_t>(bytes, offset + 24);
    section.size = read_pod<std::uint64_t>(bytes, offset + 32);
    section.name = section_name(bytes, name_offset, name_size, name);
    image.sections.push_back(section);
    if (section.type == kShtSymtab || section.type == kShtDynsym) {
      const auto entry_size = read_pod<std::uint64_t>(bytes, offset + 56);
      const auto link = read_pod<std::uint32_t>(bytes, offset + 40);
      if (entry_size == 0 || link >= section_count) {
        continue;
      }
      const std::size_t link_offset =
          static_cast<std::size_t>(section_offset) + static_cast<std::size_t>(link) * section_size;
      const auto string_offset = read_pod<std::uint64_t>(bytes, link_offset + 24);
      const auto string_size = read_pod<std::uint64_t>(bytes, link_offset + 32);
      const std::uint64_t count = section.size / entry_size;
      for (std::uint64_t symbol_index = 0; symbol_index < count && symbol_index < 256; ++symbol_index) {
        const std::size_t symbol_at = static_cast<std::size_t>(section.offset + symbol_index * entry_size);
        ElfSymbol symbol;
        const auto symbol_name = read_pod<std::uint32_t>(bytes, symbol_at);
        symbol.info = read_pod<unsigned char>(bytes, symbol_at + 4);
        symbol.section_index = read_pod<std::uint16_t>(bytes, symbol_at + 6);
        symbol.value = read_pod<std::uint64_t>(bytes, symbol_at + 8);
        symbol.size = read_pod<std::uint64_t>(bytes, symbol_at + 16);
        symbol.name = section_name(bytes, string_offset, string_size, symbol_name);
        if (!symbol.name.empty()) {
          image.symbols.push_back(symbol);
        }
      }
    }
  }
  image.supported = true;
  return image;
}

Json elf_to_json(const ElfImage& image) {
  Json json = Json::object();
  json.set("supported", Json::boolean(image.supported));
  json.set("limitation", Json::string(image.limitation));
  json.set("type", Json::integer(image.type));
  json.set("machine", Json::integer(image.machine));
  json.set("entry", Json::integer(static_cast<std::int64_t>(image.entry)));
  json.set("has_dynamic", Json::boolean(image.has_dynamic));
  json.set("has_interpreter", Json::boolean(image.has_interpreter));
  Json sections = Json::array();
  for (const ElfSection& section : image.sections) {
    Json item = Json::object();
    item.set("name", Json::string(section.name));
    item.set("type", Json::integer(section.type));
    item.set("size", Json::integer(static_cast<std::int64_t>(section.size)));
    sections.push(std::move(item));
  }
  json.set("sections", std::move(sections));
  json.set("segment_count", Json::integer(static_cast<std::int64_t>(image.segments.size())));
  json.set("symbol_count", Json::integer(static_cast<std::int64_t>(image.symbols.size())));
  return json;
}

}  // namespace syslab
