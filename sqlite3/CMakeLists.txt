add_library (sqlite3 "sqlite3.c")
target_compile_definitions (sqlite3 PUBLIC
    -DSQLITE_ENABLE_FTS5
    -DSQLITE_DQS=0
    -DSQLITE_DEFAULT_MEMSTATUS=0
    -DSQLITE_LIKE_DOESNT_MATCH_BLOBS
    -DSQLITE_MAX_EXPR_DEPTH=0
    -DSQLITE_OMIT_DEPRECATED
    -DSQLITE_ENABLE_COLUMN_METADATA
    -DSQLITE_OMIT_PROGRESS_CALLBACK
    -DSQLITE_OMIT_SHARED_CACHE
    -DSQLITE_USE_ALLOCA
    -DSQLITE_OMIT_AUTOINIT)
target_include_directories (sqlite3 INTERFACE .)

add_library (SymbolTokenizer SHARED "SymbolTokenizer.cpp" "SymbolTokenizer.h")

add_executable (sqlite3cli "shell.c")
target_link_libraries (sqlite3cli PRIVATE sqlite3)
set_target_properties (sqlite3cli PROPERTIES OUTPUT_NAME "sqlite3")

add_library (DatabaseExt SHARED "sciter-sqlite.cpp" "sciter-sqlite-db.cpp" "sciter-sqlite-rs.cpp")
target_link_libraries (DatabaseExt PRIVATE sqlite3 SymbolTokenizer TaskPool)
target_include_directories (DatabaseExt PRIVATE ${SCITERSDK}/include)
target_include_directories (DatabaseExt PRIVATE ${CMAKE_CURRENT_BINARY_DIR})