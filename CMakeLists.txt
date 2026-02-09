cmake_minimum_required(VERSION 3.16)
project(emulator CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Common compiler flags matching the original Makefile
add_compile_options(-Wall -Wextra -Wpedantic -fPIC)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Profile")
    add_compile_options(-O3 -DNDEBUG -pg -march=native -flto)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg -flto")
endif()

# Find dependencies
find_package(PkgConfig REQUIRED)
find_package(Curses REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
find_package(Threads REQUIRED)

add_subdirectory(src)

enable_testing()
add_subdirectory(test)

# Auto-update .clangd for VSCode clangd
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    if(NOT DEFINED CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CURRENT_PRESET "debug")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CURRENT_PRESET "release")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Profile")
        set(CURRENT_PRESET "profile")
    else()
        set(CURRENT_PRESET "debug")
    endif()

    file(WRITE "${CMAKE_SOURCE_DIR}/.clangd" 
        "CompileFlags:\n  CompilationDatabase: build/${CURRENT_PRESET}\n")
    message(STATUS "Updated .clangd for preset: ${CURRENT_PRESET}")
endif()
