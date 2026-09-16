cmake_minimum_required(VERSION 3.28)

project(AOIProject VERSION 0.1.0 LANGUAGES CXX)

option(AOI_BUILD_SHRIMP "Build the Shrimp Qt application" ON)
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
    # Shrimp is the application this project exists to ship, so it is built by
    # default.  Two of its dependencies are conditional in conanfile.py -- Qt
    # and the private VTK package -- and a mismatch between the Conan graph and
    # this option otherwise surfaces as a generic find_package() failure inside
    # Shrimp.  Name the missing piece and the command that provides it instead.
    #
    # Turn it off with -DAOI_BUILD_SHRIMP=OFF for a core-only build of
    # TaskControl + Runtime (+ tests), which needs neither Qt nor VTK... except
    # that TaskControl does need Qt (its context stores QVariant, recipes are
    # parsed with QJsonArray), so "core-only" means -o "&:with_qt=True".
    find_package(Qt6 QUIET COMPONENTS Core)
    if(NOT Qt6_FOUND)
        message(FATAL_ERROR
            "AOI_BUILD_SHRIMP=ON but Qt is not in the Conan graph. "
            "Re-run conan install with -o \"&:with_shrimp=True\" using the same "
            "-of folder as this preset, then reconfigure.")
    endif()

    # VTK is the private package built from conan/recipes/vtk; there is no
    # ConanCenter recipe for the configuration Shrimp needs.
    find_package(VTK QUIET COMPONENTS CommonCore)
    if(NOT VTK_FOUND)
        message(FATAL_ERROR
            "AOI_BUILD_SHRIMP=ON but the private VTK package is missing from the "
            "Conan graph. Build it with `bash scripts/build-debug.sh vtk` (which "
            "runs conan create conan/recipes/vtk), then re-run the deps and "
            "configure stages.")
    endif()
    add_subdirectory(Shrimp)
endif()
