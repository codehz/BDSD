set(MSVC_DIA_SDK_DIR "$ENV{VSINSTALLDIR}DIA SDK" CACHE PATH
      "Path to the DIA SDK")

if (CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64 OR CMAKE_SYSTEM_PROCESSOR STREQUAL AMD64)
    set (DIAARCH "/amd64")
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL armv7l OR CMAKE_SYSTEM_PROCESSOR STREQUAL arm)
    set(DIAARCH "/arm")
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    set(DIAARCH "/arm64")
else ()
    set(DIAARCH "")
endif ()

add_library (DIA STATIC IMPORTED)
set_target_properties (DIA PROPERTIES IMPORTED_LOCATION ${MSVC_DIA_SDK_DIR}/lib${DIAARCH}/diaguids.lib)
target_include_directories (DIA INTERFACE ${MSVC_DIA_SDK_DIR}/include)
target_link_directories (DIA INTERFACE ${MSVC_DIA_SDK_DIR}/lib${DIAARCH})

add_library (pdb "include/pdb.h" "pdb.cpp")
target_link_libraries (pdb PRIVATE DIA common)
target_include_directories (pdb INTERFACE include)

add_custom_command (TARGET pdb POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${MSVC_DIA_SDK_DIR}/bin${DIAARCH}/msdia140.dll
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
