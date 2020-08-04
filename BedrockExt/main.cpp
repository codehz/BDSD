#undef WIN32_LEAN_AND_MEAN
#include <sciter-x.h>
#include <aux-cvt.h>
#include <Windows.h>
#include <version.h>
#include <filesystem>
#include <MicrosoftDemangle.h>
#include <TaskPool.h>
#include <regex>

#ifndef BDSDVERSION
#  define BDSDVERSION L"0.0.0"
#endif

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

struct extension : sciter::om::asset<extension> {
  sciter::string version = BDSDVERSION;
  TaskPool pool;

  extension() {}

  SOM_PASSPORT_BEGIN_EX(bedrock, extension)
  SOM_FUNCS(SOM_FUNC(demangleVtableFunction))
  SOM_PROPS(SOM_RO_PROP(version), SOM_RO_VIRTUAL_PROP(symutils, getPath))
  SOM_PASSPORT_END

  SCITER_VALUE getPath() {
    static std::filesystem::path cached = ([] {
      WCHAR DllPath[MAX_PATH] = {0};
      GetModuleFileNameW((HINSTANCE) &__ImageBase, DllPath, _countof(DllPath));
      std::filesystem::path raw{DllPath};
      raw = raw.parent_path() / "symutils.exe";
      return raw;
    })();
    return sciter::value::make_string(cached.c_str());
  }

  SCITER_VALUE demangleVtableFunction(sciter::string inp, sciter::value cb) {
    pool.AddTask([=, this] {
      try {
        aux::w2a dat{inp};
        auto str = llvm::microsoftDemangle(
            dat.c_str(), nullptr, nullptr, nullptr, nullptr,
            (llvm::MSDemangleFlags)(llvm::MSDF_NoCallingConvention | llvm::MSDF_NoAccessSpecifier));
        if (!str) return sciter::value::make_error((L"failed to demangle: " + inp).c_str());
        std::string buf{str};
        free(str);
        static std::regex re_string{
            "class std::basic_string<char, struct std::char_traits<char>, class std::allocator<char>>"};
        static std::regex re_vector{R"raw(class std::vector<(.*), class std::allocator<\1>>)raw"};
        static std::regex re_unique{R"raw(class std::unique_ptr<(.*), struct std::default_delete<\1>>)raw"};
        buf      = std::regex_replace(buf, re_string, "std::string");
        buf      = std::regex_replace(buf, re_vector, "std::vector<$1>");
        buf      = std::regex_replace(buf, re_unique, "std::unique_ptr<$1>");
        auto ret = sciter::value::make_string(buf.c_str());
        cb.call(ret);
      } catch (...) { cb.call({}); }
    });
    return {};
  }
};

extern "C" __declspec(dllexport) BOOL SCAPI SciterLibraryInit(ISciterAPI *psapi, SCITER_VALUE *plibobject) {
  _SAPI(psapi);
  static sciter::om::hasset<extension> root = new extension();

  *plibobject = sciter::value::wrap_asset(root);
  return TRUE;
}