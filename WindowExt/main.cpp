#undef WIN32_LEAN_AND_MEAN
#include <sciter-x.h>
#include <sciter-x-window.hpp>
#include <shellapi.h>
#include <Windows.h>
#include <shobjidl_core.h>
#include <commdlg.h>
#include "hiddenapi.h"

static ITaskbarList *taskbar;

namespace sciter {

HINSTANCE application::hinstance() { return GetModuleHandle(NULL); }

LRESULT window::on_message(HWINDOW hwnd, UINT msg, WPARAM wParam, LPARAM lParam, BOOL &pHandled) { return 0; }

LRESULT SC_CALLBACK
window::msg_delegate(HWINDOW hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LPVOID pParam, BOOL *pHandled) {
  window *win = static_cast<window *>(pParam);
  return win->on_message(hwnd, msg, wParam, lParam, *pHandled);
}

void window::collapse() {
  if (_hwnd) ::ShowWindow(_hwnd, SW_MINIMIZE);
}
void window::expand(bool maximize) {
  if (_hwnd) ::ShowWindow(_hwnd, maximize ? SW_MAXIMIZE : SW_NORMAL);
}
void window::dismiss() {
  if (_hwnd) ::PostMessage(_hwnd, WM_CLOSE, 0, 0);
}

window::window(UINT creationFlags, RECT frame) : _hwnd(NULL) {
  asset_add_ref();
  _hwnd = ::SciterCreateWindow(creationFlags, &frame, &msg_delegate, this, NULL);
}

} // namespace sciter

struct basewin : sciter::window {
  basewin(UINT flags) : window(flags) {}

  bool show() {
    expand();
    return true;
  }
};

struct winext : sciter::om::asset<winext> {
  winext() {}

  SOM_PASSPORT_BEGIN(winext)
  SOM_FUNCS(
      SOM_FUNC(openFile), SOM_FUNC(saveFile), SOM_FUNC(menu), SOM_FUNC(create), SOM_FUNC(blur), SOM_FUNC(maximize),
      SOM_FUNC(restore))
  SOM_PASSPORT_END

  SCITER_VALUE openFile(sciter::string filter, sciter::string title, sciter::value el) {
    HWND hwnd = 0;
    SciterGetElementHwnd((HELEMENT) el.get_object_data(), &hwnd, TRUE);
    if (hwnd == 0) return sciter::value(false);
    std::replace(filter.begin(), filter.end(), L'|', L'\0');
    filter += L'\0';
    wchar ret[MAX_PATH]{};
    OPENFILENAME param{
        .lStructSize  = sizeof(OPENFILENAME),
        .hwndOwner    = hwnd,
        .hInstance    = sciter::application::hinstance(),
        .lpstrFilter  = filter.c_str(),
        .nFilterIndex = 0,
        .lpstrFile    = ret,
        .nMaxFile     = MAX_PATH,
        .lpstrTitle   = title.c_str(),
        .Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
    };
    GetOpenFileName(&param);
    return sciter::value::from_string(ret);
  }

  SCITER_VALUE saveFile(sciter::string filter, sciter::string defext, sciter::string title, sciter::value el) {
    HWND hwnd = 0;
    SciterGetElementHwnd((HELEMENT) el.get_object_data(), &hwnd, TRUE);
    if (hwnd == 0) return sciter::value(false);
    std::replace(filter.begin(), filter.end(), L'|', L'\0');
    filter += L'\0';
    wchar ret[MAX_PATH]{};
    OPENFILENAME param{
        .lStructSize  = sizeof(OPENFILENAME),
        .hwndOwner    = hwnd,
        .hInstance    = sciter::application::hinstance(),
        .lpstrFilter  = filter.c_str(),
        .nFilterIndex = 0,
        .lpstrFile    = ret,
        .nMaxFile     = MAX_PATH,
        .lpstrTitle   = title.c_str(),
        .Flags        = OFN_NOCHANGEDIR | OFN_EXTENSIONDIFFERENT | OFN_OVERWRITEPROMPT,
        .lpstrDefExt  = defext.c_str(),
    };
    GetSaveFileName(&param);
    return sciter::value::from_string(ret);
  }

  SCITER_VALUE create(sciter::string address, SCITER_VALUE args) {
    struct context : basewin {
      SCITER_VALUE value;
      context(SCITER_VALUE value)
          : basewin(SW_GLASSY | SW_CONTROLS | SW_RESIZEABLE | SW_TITLEBAR | SW_ENABLE_DEBUG), value(value) {}

      SOM_PASSPORT_BEGIN(context)
      SOM_PROPS(SOM_PROP(value))
      SOM_FUNCS(SOM_FUNC(show))
      SOM_PASSPORT_END
    };
    sciter::om::hasset<sciter::window> win = new context(args);

    auto hwnd = win->get_hwnd();
    win->load(address.c_str());
    make_blur(hwnd);
    return sciter::value::wrap_asset(win);
  }

  SCITER_VALUE menu(sciter::string address, SCITER_VALUE args) {
    struct menuctx : basewin {
      SCITER_VALUE value;
      menuctx(SCITER_VALUE value) : basewin(SW_GLASSY | SW_TOOL | SW_ENABLE_DEBUG), value(value) {}

      SOM_PASSPORT_BEGIN_EX(menu, menuctx)
      SOM_PROPS(SOM_PROP(value))
      SOM_FUNCS(SOM_FUNC(show))
      SOM_PASSPORT_END
    };
    sciter::om::hasset<sciter::window> win = new menuctx(args);

    auto hwnd = win->get_hwnd();
    win->load(address.c_str());
    make_blur(hwnd);
    taskbar->DeleteTab(hwnd);
    return sciter::value::wrap_asset(win);
  }

  bool blur(sciter::value el) {
    HWND hwnd = 0;
    SciterGetElementHwnd((HELEMENT) el.get_object_data(), &hwnd, TRUE);
    if (hwnd == 0) return false;
    make_blur(hwnd);
    return true;
  }

  bool maximize(sciter::value el) {
    HWND hwnd = 0;
    SciterGetElementHwnd((HELEMENT) el.get_object_data(), &hwnd, TRUE);
    if (hwnd == 0) return false;
    ShowWindow(hwnd, SW_MAXIMIZE);
    return true;
  }

  bool restore(sciter::value el) {
    HWND hwnd = 0;
    SciterGetElementHwnd((HELEMENT) el.get_object_data(), &hwnd, TRUE);
    if (hwnd == 0) return false;
    ShowWindow(hwnd, SW_RESTORE);
    return true;
  }
};

extern "C" __declspec(dllexport) BOOL SCAPI SciterLibraryInit(ISciterAPI *psapi, SCITER_VALUE *plibobject) {
  CoInitialize(0);
  CoCreateInstance(CLSID_TaskbarList, 0, CLSCTX_INPROC_SERVER, IID_ITaskbarList, (void **) &taskbar);
  taskbar->HrInit();

  _SAPI(psapi);
  static sciter::om::hasset<winext> root = new winext();

  *plibobject = sciter::value::wrap_asset(root);
  return TRUE;
}