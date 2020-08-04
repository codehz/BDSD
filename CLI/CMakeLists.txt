add_executable (symutils "main.cpp" "ostream_joiner.h")
target_link_libraries (symutils elf pdb Demangler adapter sqlite3 SymbolTokenizer)