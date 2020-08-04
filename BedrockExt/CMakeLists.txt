add_library (BedrockExt SHARED main.cpp UI.manifest)
target_link_libraries (BedrockExt PRIVATE Demangler elf pdb adapter sqlite3 TaskPool)
target_include_directories (BedrockExt PRIVATE ${SCITERSDK}/include)
target_include_directories (BedrockExt PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

configure_file (
  ${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  @ONLY)
configure_file (
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.h
  @ONLY)

target_sources (BedrockExt PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/version.rc)