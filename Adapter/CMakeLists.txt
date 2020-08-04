add_library (adapter "itanium.cpp" "include/adapter.h" "msvc.cpp")
target_link_libraries (adapter PRIVATE Demangler)
target_include_directories (adapter INTERFACE include)