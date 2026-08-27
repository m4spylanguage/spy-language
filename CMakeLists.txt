cmake_minimum_required(VERSION 3.10)
project(spy_compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SpyUI Framework (static library)
add_library(spyui STATIC
    spyui/spyui.cpp
)

target_include_directories(spyui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/spyui
)

if(WIN32)
    target_link_libraries(spyui PRIVATE gdi32 user32 shell32 comctl32 comdlg32 shlwapi)
endif()

# Spy Compiler
add_executable(spy
    src/main.cpp
    src/Token.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Codegen.cpp
    src/CppCodegen.cpp
    src/IrCodegen.cpp
)

target_include_directories(spy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/spyui
)

target_link_libraries(spy PRIVATE spyui)
