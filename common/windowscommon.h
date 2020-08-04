#pragma once

#include <windows.h>

namespace common {

struct SysHandle {
  HANDLE handle{INVALID_HANDLE_VALUE};
  char const *desc;
  SysHandle(char const *desc) : desc(desc) {}

  void operator=(HANDLE rhs) {
    if (rhs == INVALID_HANDLE_VALUE) throw std::system_error{(int) GetLastError(), std::system_category(), desc};
    handle = rhs;
  }

  ~SysHandle() {
    if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
  }

  operator HANDLE() const { return handle; }
};

class WindowsFileMapping {
  SysHandle file{"Failed to open file"};
  SysHandle mapping{"Failed to map file"};
  template <typename T> friend class MappingView;

public:
  WindowsFileMapping(WindowsFileMapping const &) = delete;
  WindowsFileMapping(std::filesystem::path const &path) {
    file = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD high, low;
    low     = GetFileSize(file, &high);
    mapping = CreateFileMapping(file, NULL, PAGE_READONLY, high, low, NULL);
  }
  WindowsFileMapping &operator=(WindowsFileMapping const &) = delete;
};

template <typename T> class MappingView {
  template <typename R> friend class MappingView;
  void *raw{};
  T *ptr{};
  size_t num{};

  static DWORD GetSysGran() {
    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);
    return SysInfo.dwAllocationGranularity;
  }

public:
  MappingView() {}
  MappingView(MappingView const &rhs) = delete;
  MappingView(MappingView &&rhs) noexcept : raw(rhs.raw), ptr(rhs.ptr), num(rhs.num) {
    rhs.raw = nullptr;
    rhs.ptr = nullptr;
  }
  template <typename R>
  MappingView(MappingView<R> &&rhs) noexcept : raw(rhs.raw), ptr((T *) rhs.ptr), num(rhs.num * sizeof(R) / sizeof(T)) {
    rhs.raw = nullptr;
    rhs.ptr = nullptr;
  }
  MappingView(WindowsFileMapping const &map) : num(0) {
    raw = MapViewOfFile(map.mapping, FILE_MAP_READ, 0, 0, 0);
    if (raw == nullptr) throw std::system_error{(int) GetLastError(), std::system_category(), "Failed to map view"};
    ptr = (T *) raw;
  }
  MappingView(WindowsFileMapping const &map, DWORD offset, size_t num = 1) : num(num) {
    static DWORD SysGram = GetSysGran();
    DWORD FileMapStart   = (offset / SysGram) * SysGram;
    DWORD MapViewSize    = (offset % SysGram) + num * sizeof(T);
    DWORD Delta          = offset - FileMapStart;

    raw = MapViewOfFile(map.mapping, FILE_MAP_READ, 0, FileMapStart, MapViewSize);
    if (raw == nullptr) throw std::system_error{(int) GetLastError(), std::system_category(), "Failed to map view"};
    ptr = (T *) ((char *) raw + Delta);
  }

  MappingView &operator=(MappingView &&rhs) noexcept {
    if (raw) UnmapViewOfFile(raw);
    raw     = rhs.raw;
    ptr     = rhs.ptr;
    num     = rhs.num;
    rhs.raw = nullptr;
    rhs.ptr = nullptr;
    return *this;
  }
  MappingView &operator=(MappingView const &rhs) = delete;

  ~MappingView() {
    if (raw) UnmapViewOfFile(raw);
  }

  T &operator*() { return *ptr; }
  T *operator->() { return ptr; }
  T &operator[](size_t idx) { return ptr[idx]; }
  T *begin() { return ptr; }
  T *end() { return ptr + num; }
};

} // namespace common