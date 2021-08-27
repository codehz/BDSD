#include "include/pdb.h"

#include <system_error>
#include <dia2.h>
#include <atlbase.h>

using pDllGetClassObject = HRESULT (*)(REFCLSID rclsid, REFIID riid, LPVOID ppv);

namespace pdb {

class HResultCollector {
public:
  void operator=(HRESULT res) {
    if (FAILED(res)) throw DumpError(std::system_category().message(res));
  }
};

class BStrHelper {
  BSTR value;

public:
  ~BStrHelper() {
    if (value) SysFreeString(value);
  }
  operator std::wstring() { return {value}; }
  operator std::string() {
    std::string ret;
    auto len = WideCharToMultiByte(CP_ACP, 0, value, SysStringLen(value), NULL, 0, NULL, NULL);
    ret.resize(len);
    WideCharToMultiByte(CP_ACP, 0, value, SysStringLen(value), ret.data(), len, NULL, NULL);
    return ret;
  }
  BSTR *operator&() { return &value; }
};

class SymbolIterator : public ISymbolIterator {
  friend class DumpSource;
  CComPtr<IDiaEnumSymbolsByAddr> byaddr;
  CComPtr<IDiaSymbol> psym;

public:
  virtual Symbol Get() override {
    BStrHelper cache;
    DWORD offset;
    psym->get_name(&cache);
    psym->get_addressOffset(&offset);
    return Symbol{.Name = cache, .Offset = offset};
  }

  virtual bool Next() override {
    HResultCollector hr;
    ULONG celt = 0;
    psym       = 0;
    hr         = byaddr->Next(1, &psym, &celt);
    DWORD Tag  = 0;
    psym->get_symTag(&Tag);
    if(celt && Tag == 10) //SymTagPublicSymbol
      return Next();
    return celt;
  }
};

class DumpSource : public IDumpSource {
  friend class PDBDump;
  CComPtr<IDiaDataSource> source;
  CComPtr<IDiaSession> session;
  void Load(std::filesystem::path const &path) {
    HResultCollector hr;
    hr = source->loadDataFromPdb(path.c_str());
    hr = source->openSession(&session);
  }

public:
  virtual std::unique_ptr<ISymbolIterator> GetIterator() override {
    HResultCollector hr;
    auto ret = std::make_unique<SymbolIterator>();
    hr       = session->getSymbolsByAddr(&ret->byaddr);
    hr       = ret->byaddr->symbolByAddr(1, 0, &ret->psym);
    return ret;
  }
};

class PDBDump : public ISymbolDumper {
  CComPtr<IClassFactory> ClassFactory;

public:
  PDBDump() {
    HResultCollector hr;
    CoInitialize(nullptr);
    WCHAR exe[MAX_PATH];
    GetModuleFileName(NULL, exe, MAX_PATH);

    std::filesystem::path path{exe};
    path        = path.parent_path() / "msdia140.dll";
    auto handle = LoadLibrary(path.c_str());
    if (handle == 0) throw DumpError{"Failed to load msdia140.dll"};
    auto getClassObj = (pDllGetClassObject) GetProcAddress(handle, "DllGetClassObject");
    if (getClassObj == 0) throw DumpError{"Failed to load DllGetClassObject from msdia140.dll"};
    hr = getClassObj(CLSID_DiaSource, IID_IClassFactory, &ClassFactory);
  }
  virtual std::unique_ptr<IDumpSource> Open(std::filesystem::path const &path) override {
    HResultCollector hr;
    auto ret = std::make_unique<DumpSource>();
    hr       = ClassFactory->CreateInstance(0, IID_IDiaDataSource, (void **) &ret->source);
    ret->Load(path);
    return std::move(ret);
  }
};

ISymbolDumper &GetDumper() {
  static PDBDump ret;
  return ret;
}

} // namespace pdb
