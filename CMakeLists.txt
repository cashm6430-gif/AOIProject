cmake_minimum_required(VERSION 3.28)

project(AOIProject VERSION 0.1.0 LANGUAGES CXX)

option(AOI_BUILD_SHRIMP "Build the Shrimp Qt application" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Every target and every Conan package uses MSVC's dynamic CRT (/MD or /MDd).
# Do not override this per target: doing so would create cross-module C++ ABI risks.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/utf-8 /permissive- /Zc:__cplusplus)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

add_subdirectory(TaskControl)

if(AOI_BUILD_SHRIMP)
    add_subdirectory(Shrimp)
endif()
