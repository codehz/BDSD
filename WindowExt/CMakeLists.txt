add_library (WindowExt SHARED main.cpp "hiddenapi.h")
target_link_libraries (WindowExt PRIVATE Dwmapi)
target_include_directories (WindowExt PRIVATE ${SCITERSDK}/include)
target_include_directories (WindowExt PRIVATE ${CMAKE_CURRENT_BINARY_DIR})