cmake_minimum_required(VERSION 3.21)

project(Listless
    VERSION 0.0.1
    DESCRIPTION "A portable file and directory viewer"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

option(LISTLESS_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(LISTLESS_BUILD_TESTS "Build the test suite" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)

add_library(listless_warnings INTERFACE)
listless_set_warnings(listless_warnings)

add_library(listless_sanitizers INTERFACE)
if(LISTLESS_ENABLE_SANITIZERS)
    target_compile_options(listless_sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(listless_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

add_subdirectory(src)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(platform/linux)
else()
    message(FATAL_ERROR "Listless has no platform backend for ${CMAKE_SYSTEM_NAME} yet")
endif()

if(LISTLESS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
