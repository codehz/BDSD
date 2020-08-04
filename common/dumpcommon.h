#pragma once

#include <stdexcept>
#include <cstdint>
#include <string>

namespace common {

class DumpError : std::runtime_error {
  using runtime_error::runtime_error;
};

struct Symbol {
  std::string Name;
  uint64_t Offset;
};

class ISymbolIterator {
public:
  virtual ~ISymbolIterator() {}
  virtual Symbol Get() = 0;
  virtual bool Next()  = 0;
};

class IDumpSource {
public:
  virtual ~IDumpSource() {}
  virtual std::unique_ptr<ISymbolIterator> GetIterator() = 0;
};

class ISymbolDumper {
public:
  virtual ~ISymbolDumper() {}
  virtual std::unique_ptr<IDumpSource> Open(std::filesystem::path const &path) = 0;
};

} // namespace common