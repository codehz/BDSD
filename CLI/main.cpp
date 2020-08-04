#include <elf.h>
#include <pdb.h>
#include <Demangle.h>
#include <ItaniumDemangle.h>
#include <MicrosoftDemangle.h>
#include <MicrosoftDemangleNodes.h>
#include <adapter.h>
#include <Windows.h>
#include <objbase.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <sqlite3.h>
#include <SymbolTokenizer.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <span>
#include <windowscommon.h>

enum struct FileType { PdbFile, ElfFile, UnknownFile };
enum struct DecodeMode { Raw, Simple, Original };

void dumpELF(std::filesystem::path const &file, DecodeMode mode) {
  auto dumper = elf::GetDumper().Open(file);
  auto it     = dumper->GetIterator();
  do {
    auto sym = it->Get();
    if (sym.Offset) {
      auto start = sym.Name.c_str();
      auto end   = start + sym.Name.length();
      if (mode == DecodeMode::Original) {
        std::cout << start << std::endl;
      } else if (mode == DecodeMode::Simple) {
        llvm::itanium_demangle::ManglingParser<llvm::itanium_demangle::DefaultAllocator> parser{start, end};
        if (auto node = parser.parse()) {
          auto cvt = adapter::Adapt(*node);
          std::cout << start << std::endl;
          std::cout << *cvt << std::endl;
        }
      } else {
        std::cout << llvm::demangle({start, end}) << std::endl;
      }
    }
  } while (it->Next());
}

void dumpPDB(std::filesystem::path const &file, DecodeMode mode) {
  auto dumper = pdb::GetDumper().Open(file);
  auto it     = dumper->GetIterator();
  do {
    auto sym = it->Get();
    if (sym.Offset) {
      auto start = sym.Name.c_str();
      auto end   = start + sym.Name.length();
      if (mode == DecodeMode::Original) {
        std::cout << start << std::endl;
      } else if (mode == DecodeMode::Simple) {
        llvm::ms_demangle::Demangler dem{};
        llvm::StringView sv{start, end};
        if (auto node = dem.parse(sv)) {
          auto cvt = adapter::Adapt(*node);
          std::cout << *cvt << std::endl;
        }
      } else {
        std::cout << llvm::demangle({start, end}) << std::endl;
      }
    }
  } while (it->Next());
}

void printHelp() {
  std::wcerr << L"symutils" << std::endl << std::endl;
  std::wcerr << L"\thelp                             Print this message." << std::endl;
  std::wcerr << L"\telf-sections <source>            Print elf sections for elf." << std::endl;
  std::wcerr << L"\tdump <source>                    Dump symbol in raw form from pdb or elf." << std::endl;
  std::wcerr << L"\tdump-decode <source>             Dump symbol in simple form from pdb or elf." << std::endl;
  std::wcerr << L"\tdump-decode-original <source>    Dump symbol in original form from pdb or elf." << std::endl;
  std::wcerr << L"\tdecode [symbol]                  Decode symbol in simple form if possible." << std::endl;
  std::wcerr << L"\tdecode-original [symbol]         Decode symbol in original form if possible." << std::endl;
  std::wcerr << L"\tbuild-database <out> <pdb> <elf> Build database from pdb and elf files." << std::endl;
}

int unknownCommand(wchar_t const *cmd) {
  std::wcerr << L"Unknown command " << cmd << std::endl;
  return 2;
}

FileType detectFileType(std::ifstream &file) {
  char sig[4];
  file.read(sig, 4);
  if (!file) throw std::runtime_error{"Failed to read file"};
  if (strncmp(sig, "Micr", 4) == 0) return FileType::PdbFile;
  if (strncmp(
          sig,
          "\x7F"
          "ELF",
          4) == 0)
    return FileType::ElfFile;
  return FileType::UnknownFile;
}

void dump(std::filesystem::path path, DecodeMode mode = DecodeMode::Raw) {
  std::ifstream ifs{path};
  if (!ifs) throw std::runtime_error{"Failed to open file"};
  auto type = detectFileType(ifs);
  ifs.close();
  switch (type) {
  case FileType::PdbFile: dumpPDB(path, mode); break;
  case FileType::ElfFile: dumpELF(path, mode); break;
  case FileType::UnknownFile: throw std::runtime_error{"Unknown source file."};
  default: break;
  }
}

// since all symbol is ascii, so no need to use complex convert logic
std::string fastcvt(wchar_t const *wstr) {
  std::string ret;
  wchar_t wc;
  while ((wc = *wstr++)) ret += (char) wc;
  return ret;
}

std::string decodePdb(std::string const &str) {
  llvm::ms_demangle::Demangler dem{};
  llvm::StringView sv{str.data(), str.data() + str.size()};
  if (auto node = dem.parse(sv)) {
    auto cvt = adapter::Adapt(*node);
    std::ostringstream oss;
    oss << *cvt;
    return oss.str();
  }
  return str;
}

std::string decodeElf(std::string const &str) {
  llvm::itanium_demangle::ManglingParser<llvm::itanium_demangle::DefaultAllocator> parser{
      str.data(), str.data() + str.size()};
  if (auto node = parser.parse()) {
    auto cvt = adapter::Adapt(*node);
    std::ostringstream oss;
    oss << *cvt;
    return oss.str();
  }
  return str;
}

void do_DecodeSymbol(wchar_t const *wstr, DecodeMode mode) {
  auto str = fastcvt(wstr);
  if (str.empty()) return;
  if (mode == DecodeMode::Original) {
    std::cout << llvm::demangle(str) << std::endl;
    return;
  }
  switch (str[0]) {
  case '?': std::cout << decodePdb(str) << std::endl; break;
  case '_': std::cout << decodeElf(str) << std::endl; break;
  default: std::cout << str << std::endl;
  }
}

void loop_DecodeSymbol(DecodeMode mode) {
  std::wstring buffer;
  while (std::getline(std::wcin, buffer)) do_DecodeSymbol(buffer.c_str(), mode);
}

struct sqlerr {
  sqlite3 *&db;
  char **errmsg = nullptr;
  sqlerr(sqlite3 *&db, char **errmsg = nullptr) : db(db), errmsg(errmsg) {}

  void operator=(int res) {
    if (res == SQLITE_OK) return;
    if (errmsg && *errmsg) throw std::runtime_error{*errmsg};
    throw std::runtime_error{sqlite3_errmsg(db)};
  }
};

class stopwatch {
  bool has_skip = false;
  std::chrono::milliseconds dur;
  std::chrono::steady_clock::time_point time_point = std::chrono::steady_clock::now();
  int count = 0, last_count = 0, skip = 0;

  void check() {
    auto now = std::chrono::steady_clock::now();
    if (now - time_point >= dur) {
      float diff = (float) std::chrono::duration_cast<std::chrono::milliseconds>(now - time_point).count();
      while (now - time_point >= dur) time_point += dur;
      if (has_skip)
        std::cerr << "inserted " << count << " (+" << (count - last_count) << ", "
                  << ((float) (count - last_count) / diff * 1000.0) << " /s, " << skip << " skipped, "
                  << ((float) skip / (count - last_count + skip) * 100) << "%)." << std::endl;
      else
        std::cerr << "inserted " << count << " (+" << (count - last_count) << ", "
                  << ((float) (count - last_count) / diff * 1000.0) << " /s)." << std::endl;
      skip       = 0;
      last_count = count;
    }
  }

public:
  stopwatch(std::chrono::milliseconds dur, bool has_skip = true) : dur(dur), has_skip(has_skip) {}

  void reset() {
    time_point = std::chrono::steady_clock::now();
    count      = 0;
    last_count = 0;
    skip       = 0;
  }
  void add_count() {
    count++;
    check();
  }
  void add_skip() {
    skip++;
    check();
  }
  int get_count() { return count; }
};

struct DatabaseBuilder {
  enum struct ClassKind {
    NoInherit,
    SingleInherit,
    VirtualMulitiInherit,
  };

  struct ClassTypeInfo {
    struct FillContext {
      sqlite3 *db;
      sqlite3_stmt *root{}, *child{};

      FillContext(sqlite3 *db) : db(db) {
        sqlerr{db} = sqlite3_prepare_v3(
            db, "INSERT INTO typeinfos VALUES(?, ?, ?);", -1, SQLITE_PREPARE_PERSISTENT, &root, nullptr);
        sqlerr{db} = sqlite3_prepare_v3(
            db, "INSERT INTO typeinfo_defs VALUES(?, ?, ?, ?);", -1, SQLITE_PREPARE_PERSISTENT, &child, nullptr);
      }

      ~FillContext() {
        sqlite3_finalize(root);
        sqlite3_finalize(child);
      }

      void FillRoot(uint64_t base, int type, uint64_t flags) {
        sqlite3_reset(root);
        sqlite3_clear_bindings(root);
        sqlerr{db} = sqlite3_bind_int64(root, 1, (int64_t) base);
        sqlerr{db} = sqlite3_bind_int(root, 2, type);
        sqlerr{db} = sqlite3_bind_int64(root, 3, (int64_t) flags);
        if (auto res = sqlite3_step(root); res != SQLITE_DONE) sqlerr{db} = res;
      }

      void FillChild(uint64_t base, uint64_t target, int64_t offset, int flags) {
        sqlite3_reset(child);
        sqlite3_clear_bindings(child);
        sqlerr{db} = sqlite3_bind_int64(child, 1, (int64_t) base);
        sqlerr{db} = sqlite3_bind_int64(child, 2, (int64_t) target);
        sqlerr{db} = sqlite3_bind_int64(child, 3, offset);
        sqlerr{db} = sqlite3_bind_int(child, 4, flags);
        if (auto res = sqlite3_step(child); res != SQLITE_DONE) sqlerr{db} = res;
      }
    };

    virtual ~ClassTypeInfo()                           = default;
    virtual void Fill(uint64_t root, FillContext &ctx) = 0;
  };

  struct NoInheritClassTypeInfo : ClassTypeInfo {
    NoInheritClassTypeInfo() = default;
    virtual void Fill(uint64_t root, FillContext &ctx) override { ctx.FillRoot(root, 0, 0); }
  };

  struct SingleInheritClassTypeInfo : ClassTypeInfo {
    uint64_t base;
    SingleInheritClassTypeInfo(uint64_t base) : base(base) {}

    virtual void Fill(uint64_t root, FillContext &ctx) override {
      ctx.FillRoot(root, 1, 0);
      ctx.FillChild(root, base, 0, 0);
    }
  };

  struct VirtualMultiInheritClassTypeInfo : ClassTypeInfo {
    uint64_t flags{};
    struct ClassDesc {
      uint64_t base{};
      int64_t offset_flags{};
    };
    std::vector<ClassDesc> bases;

    VirtualMultiInheritClassTypeInfo() = default;

    virtual void Fill(uint64_t root, FillContext &ctx) override {
      ctx.FillRoot(root, 2, flags);
      for (auto base : bases) { ctx.FillChild(root, base.base, base.offset_flags >> 8, base.offset_flags & 0xFF); }
    }
  };

  std::filesystem::path const &out;
  std::filesystem::path const &pdb;
  std::filesystem::path const &elf;
  sqlite3 *db{};
  sqlite3_stmt *stmt{}, *vtable_stmt{};
  std::optional<elf::SectionData> rodata;
  std::map<uint64_t, std::span<uint64_t>> vtables;
  std::map<uint64_t, std::unique_ptr<ClassTypeInfo>> typeinfos;
  char *errmsg = nullptr;
  std::ostringstream oss;
  std::unordered_map<uint64_t, ClassKind> relocmap;
  std::unordered_set<uint64_t> pureset;

  DatabaseBuilder(DatabaseBuilder const &) = delete;

  DatabaseBuilder(std::filesystem::path const &out, std::filesystem::path const &pdb, std::filesystem::path const &elf)
      : out(out), pdb(pdb), elf(elf) {}

  ~DatabaseBuilder() {
    if (stmt) sqlerr{db} = sqlite3_finalize(stmt);
    if (vtable_stmt) sqlerr{db} = sqlite3_finalize(vtable_stmt);
    if (db) sqlerr{db} = sqlite3_close(db);
  }

  void operator()() {
    std::cerr << "open pdb file..." << std::endl;
    auto pdb_dumper = pdb::GetDumper().Open(pdb);
    std::cerr << "open elf file..." << std::endl;
    auto elf_dumper = elf::GetDumper().Open(elf);

    std::cerr << "prepare relocation table for type info..." << std::endl;
    auto &dumper = *(elf::IElfDumpSource *) elf_dumper.get();
    rodata       = dumper.GetSection(".data.rel.ro");
    if (!rodata) throw std::runtime_error{"Failed to load .data.rel.ro section"};
    {
      auto dynstr = dumper.GetSection(".dynstr");
      if (!dynstr) throw std::runtime_error{"Failed to load .dynsym section"};
      dynstr->diff = 0;

      auto dynsym = dumper.GetSection(".dynsym");
      if (!dynsym) throw std::runtime_error{"Failed to load .dynsym section"};
      common::MappingView<elf::symbol_data> esyms = std::move(dynsym->data);

      auto rela = dumper.GetSection(".rela.dyn");
      if (!rela) throw std::runtime_error{"Failed to load .rela.dyn section"};
      common::MappingView<elf::elf_rela> erela = std::move(rela->data);

      uint64_t pure = 0;
      uint64_t vmi  = 0;
      uint64_t si   = 0;
      uint64_t ni   = 0;

      for (auto &sym : esyms) {
        auto str = dynstr->GetMapped(sym.st_name);
        if (sym.st_value == 0) {
          if (strncmp("_ZTVN10__cxxabiv1", str, 17) == 0) {
            if (strncmp("21__vmi_class_type_infoE", str + 17, 24) == 0) {
              vmi = &sym - esyms.begin();
            } else if (strncmp("20__si_class_type_infoE", str + 17, 23) == 0) {
              si = &sym - esyms.begin();
            } else if (strncmp("17__class_type_infoE", str + 17, 20) == 0) {
              ni = &sym - esyms.begin();
            }
          } else if (strcmp("__cxa_pure_virtual", str) == 0) {
            pure = &sym - esyms.begin();
          }
        }
      }

      if (vmi == 0 || si == 0 || ni == 0 || pure == 0) throw std::runtime_error{"Failed to found vmi or si type info"};

      for (auto &rela : erela) {
        if (rela.r_type != 1) continue;
        if (rela.r_sym == ni)
          relocmap[rela.offset] = ClassKind::NoInherit;
        else if (rela.r_sym == si)
          relocmap[rela.offset] = ClassKind::SingleInherit;
        else if (rela.r_sym == vmi)
          relocmap[rela.offset] = ClassKind::VirtualMulitiInherit;
        else if (rela.r_sym == pure)
          pureset.insert(rela.offset);
      }
    }

    std::cerr << "create iterators..." << std::endl;
    auto pdb_iterator = pdb_dumper->GetIterator();
    auto elf_iterator = elf_dumper->GetIterator();

    initialDatabase();

    std::cerr << "fill symbols from elf file..." << std::endl;
    fillSymbols<&DatabaseBuilder::elfsymbol, 2>(*elf_iterator);
    std::cerr << "fill vtables from elf file..." << std::endl;
    fillVtables();
    std::cerr << "fill typeinfos from elf file..." << std::endl;
    fillTypeinfos();
    std::cerr << "fill symbols from pdb file..." << std::endl;
    fillSymbols<&DatabaseBuilder::mssymbol, 1>(*pdb_iterator);

    sort();

    std::cerr << "rebuild fts5 index..." << std::endl;
    sql("INSERT INTO fts_symbols(fts_symbols) VALUES('rebuild')");

    std::cerr << "commit..." << std::endl;
    sql("COMMIT;");
    std::cerr << "vacuum..." << std::endl;
    sql("VACUUM;");
  }

  void sql(char const *sql) { sqlerr{db, &errmsg} = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg); }

  void initialDatabase() {
    std::cerr << "open database..." << std::endl;
    sqlerr{db} = sqlite3_open16((void const *) out.c_str(), &db);
    std::cerr << "initializing database..." << std::endl;
    sql("PRAGMA journal_mode = WAL;");
    sql("PRAGMA synchronous = NORMAL;");
    sql("PRAGMA temp_store = FILE;");
    sql("BEGIN;");
    sql("DROP TABLE IF EXISTS fts_symbols;");
    sql("DROP TABLE IF EXISTS symbols;");
    sql("DROP TABLE IF EXISTS vtables;");
    sql("DROP TABLE IF EXISTS typeinfos;");
    sql("DROP TABLE IF EXISTS typeinfo_defs;");
    sql("CREATE TABLE typeinfos(key INTEGER PRIMARY KEY, type INTEGER, flags INTEGER);");
    sql("CREATE TABLE typeinfo_defs(key INT, target INT, offset INT, flags INTEGER);");
    sql("CREATE INDEX typeinfo_defs_index ON typeinfo_defs(key);");
    sql("CREATE INDEX typeinfo_defs_target_index ON typeinfo_defs(target);");
    sql("CREATE TABLE vtables(key INT, idx INT, target INT, PRIMARY KEY(key, idx));");
    sql("CREATE TABLE symbols(key TEXT, raw TEXT, type INT, original INT, offset INT);");
    sql("CREATE VIRTUAL TABLE fts_symbols USING FTS5("
        "key, raw UNINDEXED, type UNINDEXED, original UNINDEXED, offset UNINDEXED, "
        "content='symbols', tokenize='symbol');");
    sql("CREATE INDEX symbol_index ON symbols(key);");
    sql("CREATE INDEX symbol_offset_index ON symbols(offset);");
    sql("CREATE TEMP TABLE symbols_unsorted (key TEXT, raw TEXT, type INT, original INT, offset INT);");
    sql("CREATE INDEX unsorted_symbol_index ON symbols_unsorted(key);");

    sqlerr{db} = sqlite3_prepare_v3(
        db, "INSERT INTO symbols_unsorted VALUES (?, ?, ?, ?, ?);", -1, SQLITE_PREPARE_PERSISTENT, &stmt, nullptr);
    sqlerr{db} = sqlite3_prepare_v3(
        db, "INSERT INTO vtables VALUES (?, ?, ?);", -1, SQLITE_PREPARE_PERSISTENT, &vtable_stmt, nullptr);
  }

  void sort() {
    std::cerr << "sorting symbols..." << std::endl;
    sql("INSERT INTO symbols SELECT * FROM symbols_unsorted ORDER BY key;");
    sql("DROP INDEX unsorted_symbol_index;");
    sql("DROP TABLE symbols_unsorted;");
  }

  bool mssymbol(common::Symbol const &sym, int &type) {
    llvm::ms_demangle::Demangler dem{};
    llvm::StringView sv{sym.Name.begin()._Ptr, sym.Name.end()._Ptr};
    auto node = dem.parse(sv);
    if (!node) return false;
    auto cvt = adapter::Adapt(*node);
    type     = (int) cvt->Kind;
    oss << *cvt;
    if (isSkiped()) return false;
    return true;
  }

  bool elfsymbol(common::Symbol const &sym, int &type) {
    llvm::itanium_demangle::ManglingParser<llvm::itanium_demangle::DefaultAllocator> parser{
        sym.Name.begin()._Ptr, sym.Name.end()._Ptr};
    auto node = parser.parse();
    if (!node) return false;
    auto cvt = adapter::Adapt(*node);
    type     = (int) cvt->Kind;
    oss << *cvt;
    if (isSkiped()) return false;
    if (auto sp = dynamic_cast<adapter::SpecialNameNode *>(cvt.get())) {
      if (sp->Kind == adapter::SpecialNameKind::vtable) {
        auto start = (uint64_t *) rodata->GetMapped(sym.Offset);
        if (start) {
          auto step = start + 2;
          while (*step || pureset.contains(rodata->GetOffset((char *) step))) step++;
          vtables.insert_or_assign(sym.Offset, std::span{start + 2, step});
        }
      } else if (sp->Kind == adapter::SpecialNameKind::type_info) {
        if (auto classtype = relocmap.find(sym.Offset); classtype != relocmap.end()) {
          auto start = (uint64_t *) rodata->GetMapped(sym.Offset);
          start += 2; // skip type name
          switch (classtype->second) {
          case ClassKind::NoInherit: typeinfos.emplace(sym.Offset, std::make_unique<NoInheritClassTypeInfo>()); break;
          case ClassKind::SingleInherit:
            typeinfos.emplace(sym.Offset, std::make_unique<SingleInheritClassTypeInfo>(*start));
            break;
          case ClassKind::VirtualMulitiInherit: {
            auto temp = std::make_unique<VirtualMultiInheritClassTypeInfo>();
            struct _head {
              uint32_t flags;
              uint32_t count;
            } *head = (_head *) start;
            start++;
            temp->flags = head->flags;
            for (uint32_t i = 0; i < head->count; i++) {
              VirtualMultiInheritClassTypeInfo::ClassDesc desc;
              desc.base         = *start++;
              desc.offset_flags = *start++;
              temp->bases.emplace_back(std::move(desc));
            }
            typeinfos.emplace(sym.Offset, std::move(temp));
          } break;
          default: break;
          }
        }
      }
    }
    return true;
  }

  bool isSkiped() {
    return oss.str().starts_with("std::") || oss.str().starts_with("grpc::") || oss.str().starts_with("grpc_core::") ||
           oss.str().starts_with("google::") || oss.str().starts_with("__gnu_cxx::") ||
           oss.str().starts_with("JsonUtil::") || oss.str().starts_with("(") || oss.str().starts_with("$SKIP");
  }

  void fillVtables() {
    using namespace std::chrono_literals;
    stopwatch watch(2s, false);
    for (auto &[k, v] : vtables) {
      for (auto &a : v) {
        auto idx   = &a - v.data();
        sqlerr{db} = sqlite3_bind_int64(vtable_stmt, 1, (int64_t) k);
        sqlerr{db} = sqlite3_bind_int(vtable_stmt, 2, (int) idx);
        sqlerr{db} = sqlite3_bind_int64(vtable_stmt, 3, (int64_t) a);
        if (auto res = sqlite3_step(vtable_stmt); res != SQLITE_DONE) sqlerr{db} = res;
        sqlite3_reset(vtable_stmt);
        sqlite3_clear_bindings(vtable_stmt);
      }
      watch.add_count();
    }

    std::cerr << "filled " << watch.get_count() << " vtable entry." << std::endl;
  }

  void fillTypeinfos() {
    using namespace std::chrono_literals;
    stopwatch watch(2s, false);
    ClassTypeInfo::FillContext ctx{db};
    for (auto &[k, v] : typeinfos) {
      v->Fill(k, ctx);
      watch.add_count();
    }
    std::cerr << "filled " << watch.get_count() << " type_info entry." << std::endl;
  }

  template <bool (DatabaseBuilder::*Decoder)(common::Symbol const &sym, int &type), int original>
  void fillSymbols(common::ISymbolIterator &it) {
    using namespace std::chrono_literals;

    stopwatch watch(2s);

    do {
      oss.str("");
      oss.clear();
      auto symbol = it.Get();
      int type;
      if (symbol.Offset == 0 || !(this->*Decoder)(symbol, type)) {
        watch.add_skip();
        continue;
      }
      sqlerr{db} = sqlite3_bind_text(stmt, 1, oss.str().c_str(), (int) oss.str().length(), SQLITE_STATIC);
      sqlerr{db} = sqlite3_bind_text(stmt, 2, symbol.Name.c_str(), (int) symbol.Name.length(), SQLITE_STATIC);
      sqlerr{db} = sqlite3_bind_int(stmt, 3, type);
      sqlerr{db} = sqlite3_bind_int(stmt, 4, original);
      sqlerr{db} = sqlite3_bind_int64(stmt, 5, symbol.Offset);
      if (auto res = sqlite3_step(stmt); res != SQLITE_DONE) sqlerr{db} = res;
      sqlite3_reset(stmt);
      sqlite3_clear_bindings(stmt);
      watch.add_count();
    } while (it.Next());

    std::cerr << "filled " << watch.get_count() << " symbols." << std::endl;
  }
};

extern "C" __declspec(dllexport) void buildDatabase(
    std::filesystem::path const &out, std::filesystem::path const &pdb, std::filesystem::path const &elf) {
  DatabaseBuilder{out, pdb, elf}();
}

void GetElfSections(std::filesystem::path const &elf) {
  auto dumper  = elf::GetDumper().Open(elf);
  auto headers = ((elf::IElfDumpSource *) dumper.get())->GetSectionHeaders();
  std::cout << std::hex << std::setfill('0');
  for (auto header : headers)
    std::cout << std::setw(8) << header.address << "-" << std::setw(8)
              << (header.address == 0 ? 0 : header.address + header.size) << " " << std::setw(8) << header.size << " "
              << std::setw(8) << header.offset << " " << header.name << std::endl;
}

int wmain(int argc, wchar_t *argv[]) {
  try {
    CoInitialize(NULL);
    sqlite3_initialize();
    sqlite3_auto_extension((void (*)()) sqlite3_symboltokenizer_init);
    switch (argc) {
    case 1: printHelp(); return 2;
    case 2:
      if (_wcsicmp(argv[1], L"help") == 0) {
        printHelp();
      } else if (_wcsicmp(argv[1], L"decode") == 0) {
        loop_DecodeSymbol(DecodeMode::Simple);
      } else if (_wcsicmp(argv[1], L"decode-original") == 0) {
        loop_DecodeSymbol(DecodeMode::Original);
      } else
        return unknownCommand(argv[1]);
      break;
    case 3:
      if (_wcsicmp(argv[1], L"elf-sections") == 0) {
        GetElfSections(argv[2]);
      } else if (_wcsicmp(argv[1], L"dump") == 0) {
        dump(argv[2]);
      } else if (_wcsicmp(argv[1], L"dump-decode") == 0) {
        dump(argv[2], DecodeMode::Simple);
      } else if (_wcsicmp(argv[1], L"dump-decode-original") == 0) {
        dump(argv[2], DecodeMode::Original);
      } else if (_wcsicmp(argv[1], L"decode") == 0) {
        do_DecodeSymbol(argv[2], DecodeMode::Simple);
      } else if (_wcsicmp(argv[1], L"decode-original") == 0) {
        do_DecodeSymbol(argv[2], DecodeMode::Original);
      } else
        return unknownCommand(argv[1]);
      break;
    case 5:
      if (_wcsicmp(argv[1], L"build-database") == 0) {
        buildDatabase(argv[2], argv[3], argv[4]);
      } else
        return unknownCommand(argv[1]);
      break;
    default: return unknownCommand(argv[1]);
    }
    if (argc == 1) {
      printHelp();
      return 0;
    }
  } catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}