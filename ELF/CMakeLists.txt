add_library (elf  "elf.cpp" "include/elf.h")
target_link_libraries (elf PUBLIC common)
target_include_directories (elf INTERFACE include)