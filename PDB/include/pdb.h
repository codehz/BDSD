#pragma once
#include <string>
#include <memory>
#include <stdexcept>
#include <filesystem>

#include <dumpcommon.h>

namespace pdb {

using namespace common;

ISymbolDumper &GetDumper();

} // namespace pdb