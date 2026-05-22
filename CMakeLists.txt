cmake_minimum_required(VERSION 3.16)
project(Q3ARailgun C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

if(APPLE)
    add_compile_definitions(
        NO_VM_COMPILED
    )
elseif(UNIX)
    add_compile_definitions(
        LINUX
        __linux__
        NO_VM_COMPILED
    )
else()
    add_compile_definitions(
        NO_VM_COMPILED
    )
endif()

add_compile_options(
    -Wall
    -Wno-unused-result
    -Wno-format-overflow
    -fno-strict-aliasing
    -fomit-frame-pointer
)

add_subdirectory(src)
