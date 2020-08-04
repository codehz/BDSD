#pragma once

#include <memory>
#include <filesystem>
#include <optional>

#include <dumpcommon.h>
#include <windowscommon.h>

namespace elf {

using namespace common;

using elf_half_t  = uint16_t;
using elf_word_t  = uint32_t;
using elf_qword_t = uint64_t;
using elf_long_t  = int64_t;

struct SectionData {
  MappingView<char> data;
  elf_qword_t diff{};

  SectionData(MappingView<char> &&data, elf_qword_t diff) : data(std::move(data)), diff(diff) {}
  inline char *GetMapped(uint64_t va) {
    auto ret = data.begin() + va - diff;
    if (ret >= data.end()) return nullptr;
    return ret;
  }

  inline uint64_t GetOffset(char *ptr) {
    if (ptr < data.begin() || ptr >= data.end()) return 0;
    return ptr + diff - data.begin();
  }
};

struct SectionHeader {
  std::string name;
  uint64_t address, offset, size;

  SectionHeader(std::string name, uint64_t address, uint64_t offset, uint64_t size)
      : name(std::move(name)), address(address), offset(offset), size(size) {}
};

struct IElfDumpSource : IDumpSource {
  virtual std::vector<SectionHeader> GetSectionHeaders()                 = 0;
  virtual std::optional<SectionData> GetSection(std::string const &name) = 0;
};

ISymbolDumper &GetDumper();

constexpr int EI_NIDENT = 16;

struct elf_header {
  // [0-3] Magic numbers
  // [4] either 1 or 2, 32-bit or 64-bit
  // [5] either 1 or 2, little endian or big endian
  // [6] Version of ELF
  // [7] Target operating system
  // [8] Specifies ABI version
  // [9-16] Not used
  char e_ident[EI_NIDENT] = {};

  // Type of object file
  elf_half_t e_type{};

  // Target ISA
  // 0x3E is x86-64
  elf_half_t e_machine{};

  // Usually just 1 for the original ELF version
  elf_word_t e_version{};

  // Memory address for the entry point of the process
  elf_qword_t e_entry{};

  // Points to the start of the program header table
  // 0x34 for 32-bit or 0x40 for 64-bit offset
  elf_qword_t e_phoff{};

  // Points to the start of the section header table
  elf_qword_t e_shoff{};

  // Interpretation of this field depends on the target architecture.
  elf_word_t e_flags{};

  // Contains the size of this header, normally 64 Bytes for 64-bit and 52 Bytes
  // for 32-bit format.
  elf_half_t e_ehsize{};

  // Contains the size of a program header table entry.
  elf_half_t e_phentsize{};

  // Contains the number of entries in the program header table.
  elf_half_t e_phnum{};

  // Contains the size of a section header table entry.
  elf_half_t e_shentsize{};

  // Contains the number of entries in the section header table.
  elf_half_t e_shnum{};

  // Contains index of the section header table entry that contains the section
  // names.
  elf_half_t e_shstrndx{};
};

struct section_header {
  // Offset to a string in the .shstrtab section
  elf_word_t sh_name;

  // Identifies the type of this header
  elf_word_t sh_type;

  // Identfies the attributes of the section
  elf_qword_t sh_flags;

  // Virtual address of the section in memory, for sectino that are loaded
  elf_qword_t sh_addr;

  // Offset of the section in the file image
  elf_qword_t sh_offset;

  // Size in bytes of the section in the file image, can be 0
  elf_qword_t sh_size;

  // Contains the section index of an associated section
  elf_word_t sh_link;

  // Contains extra information about the section
  elf_word_t sh_info;

  // Contains the required alignment of the section
  elf_qword_t sh_addralign;

  // Contains the size, in bytes, of each entry, for sections that contain
  // fixed-size entries
  elf_qword_t sh_entsize;
};

struct symbol_data {
  // Index into the string table of the name of the symbol, or 0 for scratch
  // register
  elf_word_t st_name;

  // Register number
  char st_info;

  // Unused
  char st_other;

  // Bind is typically `STB_GLobal`
  elf_half_t st_shndx;

  elf_qword_t st_value;
  elf_qword_t st_size;
};

struct elf_rela {
  elf_qword_t offset;
  elf_word_t r_type;
  elf_word_t r_sym;
  elf_long_t r_addend;
};

enum SHT { SHT_DYNSYM = 11 };

static constexpr int elf_header_size     = sizeof(elf_header);
static constexpr int section_header_size = sizeof(section_header);

} // namespace elf