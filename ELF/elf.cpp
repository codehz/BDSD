#include "include/elf.h"

#include <cassert>
#include <fstream>
#include <memory>
#include <Windows.h>
#include <handleapi.h>
#include <windowscommon.h>

namespace elf {

struct SymbolIterator : public ISymbolIterator {
  MappingView<symbol_data> syms;
  MappingView<char> strtab;
  symbol_data *it{};
  virtual Symbol Get() override { return Symbol{.Name = &strtab[it->st_name], .Offset = it->st_value}; }
  virtual bool Next() override { return ++it < syms.end(); }
};

constexpr int ELFMAG0 = 0x7f;
constexpr int ELFMAG1 = 'E';
constexpr int ELFMAG2 = 'L';
constexpr int ELFMAG3 = 'F';

constexpr char magic_numbers[] = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3};

struct DumpSource : public IElfDumpSource {
  friend class SymbolDumper;
  WindowsFileMapping map;
  MappingView<elf_header> header;
  MappingView<section_header> sections;
  MappingView<char> shstrtab;

  DumpSource(std::filesystem::path const &path) : map(path) {
    MappingView<elf_header> header{map, 0};
    if (std::memcmp(header->e_ident, &magic_numbers, sizeof magic_numbers) != 0) throw DumpError{"Not a elf"};
    MappingView<section_header> sections{map, (DWORD) header->e_shoff, (DWORD) header->e_shnum};
    auto str       = sections[header->e_shstrndx];
    shstrtab       = {map, (DWORD) str.sh_offset, str.sh_size};
    this->header   = std::move(header);
    this->sections = std::move(sections);
  }
  virtual std::unique_ptr<ISymbolIterator> GetIterator() override {
    auto ret = std::make_unique<SymbolIterator>();
    for (auto &section : sections) {
      if (section.sh_type != SHT::SHT_DYNSYM) continue;
      unsigned long num = section.sh_size / section.sh_entsize;
      MappingView<symbol_data> syms{map, (DWORD) section.sh_offset, num};
      auto &strsec = sections[section.sh_link];
      MappingView<char> strtab{map, (DWORD) strsec.sh_offset, strsec.sh_size};

      ret->syms   = std::move(syms);
      ret->strtab = std::move(strtab);
      ret->it     = ret->syms.begin();
      return ret;
    }
    return nullptr;
  }

  virtual std::vector<SectionHeader> GetSectionHeaders() override {
    std::vector<SectionHeader> ret;
    for (auto &section : sections)
      ret.emplace_back(&shstrtab[section.sh_name], section.sh_addr, section.sh_offset, section.sh_size);
    return ret;
  }

  virtual std::optional<SectionData> GetSection(std::string const &name) override {
    for (auto &section : sections) {
      if (name == &shstrtab[section.sh_name]) {
        return SectionData{MappingView<char>{map, (DWORD) section.sh_offset, (DWORD) section.sh_size}, section.sh_addr};
      }
    }
    return std::nullopt;
  }
};

class SymbolDumper : public ISymbolDumper {
public:
  virtual std::unique_ptr<IDumpSource> Open(std::filesystem::path const &path) override {
    return std::make_unique<DumpSource>(path);
  }
};

ISymbolDumper &elf::GetDumper() {
  static SymbolDumper ret;
  return ret;
}

} // namespace elf