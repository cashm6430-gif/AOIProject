cmake_minimum_required(VERSION 3.28)

project(AOIProject VERSION 0.1.0 LANGUAGES CXX)

option(AOI_BUILD_SHRIMP "Build the Shrimp Qt application" OFF)
option(AOI_BUILD_RUNTIME "Build the headless multi-workpiece runtime" ON)
option(AOI_BUILD_TASKCONTROL_TESTS "Build TaskControl and Runtime Catch2 regression tests" OFF)

if(AOI_BUILD_TASKCONTROL_TESTS)
    include(CTest)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep generated IDE solutions aligned with the CMake target architecture.
# In Visual Studio this creates the top-level Applications and Libraries folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Every target and every Conan package uses MSVC's dynamic CRT (/MD or /MDd).
# Do not override this per target: doing so would create cross-module C++ ABI risks.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/utf-8 /permissive- /Zc:__cplusplus)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Deploy an executable's DLL closure beside it so running from the build tree
# works without manually modifying PATH. Qt plugins are not linked DLLs, so
# windeployqt also adds platforms/qwindows.dll, image formats, and similar.
function(aoi_deploy_runtime_dependencies target)
    if(NOT WIN32)
        return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:${target}>"
            $<TARGET_RUNTIME_DLLS:${target}>
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    # Conan's Qt component libraries are interface targets, while its package
    # exports windeployqt as an imported executable target.
    if(TARGET Qt6::windeployqt)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "$<TARGET_FILE:Qt6::windeployqt>"
                --no-translations
                --no-compiler-runtime
                --dir "$<TARGET_FILE_DIR:${target}>"
                "$<TARGET_FILE:${target}>"
            VERBATIM
        )
    endif()
endfunction()

add_subdirectory(TaskControl)

if(AOI_BUILD_RUNTIME)
    add_subdirectory(Runtime)
endif()

if(AOI_BUILD_SHRIMP)
    add_subdirectory(Shrimp)
endif()
