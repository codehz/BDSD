cmake_minimum_required (VERSION 3.14)

project (BDSD)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

set (BDSD_NUMBER 0)
set (BDSD_VERSION 1)
set (BDSD_BUILD_NUMBER 0)

add_compile_options (/wd26812)
add_definitions (/DWINDOWS /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /DUNICODE /DWIN32_LEAN_AND_MEAN)

set (SCITERSDK $ENV{SCITERSDK} CACHE PATH "Sciter SDK Path")
if (NOT IS_DIRECTORY "${SCITERSDK}")
  message (FATAL_ERROR "ENV:SCITERSDK NOT FOUND")
endif ()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64 OR CMAKE_SYSTEM_PROCESSOR STREQUAL AMD64)
  set (SCITERATCH "x64")
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL armv7l OR CMAKE_SYSTEM_PROCESSOR STREQUAL arm)
  message (FATAL_ERROR "ARCH NOT SUPPORT")
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
  set (SCITERATCH "arm64")
else ()
  set (SCITERATCH "x32")
endif ()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory ("common")
add_subdirectory ("TaskPool")
add_subdirectory ("Demangler")
add_subdirectory ("PDB")
add_subdirectory ("ELF")
add_subdirectory ("Adapter")
add_subdirectory ("sqlite3")
add_subdirectory ("BedrockExt")
add_subdirectory ("WindowExt")
add_subdirectory ("CLI")