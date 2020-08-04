#include "sciter-sqlite.h"
#include "sqlite3.h"
#include "SymbolTokenizer.h"

extern "C" {

#ifndef WINDOWS
__attribute__((visibility("default")))
#else
__declspec(dllexport)
#endif
BOOL SCAPI
SciterLibraryInit(ISciterAPI *psapi, SCITER_VALUE *plibobject) {
  _SAPI(psapi);
  sqlite3_initialize();
  sqlite3_auto_extension((void (*)()) sqlite3_symboltokenizer_init);
  static sciter::om::hasset<sqlite::SQLite> sqlite_root = new sqlite::SQLite(); // invloked once (C++ static convention)
  *plibobject                                           = sciter::value::wrap_asset(sqlite_root);
  return TRUE;
}
}
