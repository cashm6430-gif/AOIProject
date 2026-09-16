find_package(Qt6 REQUIRED COMPONENTS Core Gui)
find_package(Eigen3 CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Taskflow CONFIG REQUIRED)

set(AOI_TASKCONTROL_SOURCES
    Logger/XLogger.cpp
    TaskControl.cpp
    TaskControl_C.cpp
    TaskControlImpl.cpp
    core/AlgorithmContext.cpp
    core/AlgorithmTask.cpp
    core/CancellationToken.cpp
    core/OperatorRegistry.cpp
    core/OperatorGraph.cpp
    core/PipelineBuilder.cpp
    core/PluginLoader.cpp
    core/RegistrationScope.cpp
    core/SchemaValidator.cpp
    core/TaskflowExecutor.cpp
    operators/RegisterBuiltinOperators.cpp
    TaskControl.rc
)

file(GLOB_RECURSE AOI_TASKCONTROL_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# These groups are consumed by Visual Studio's CMake integration. They do not
# affect the compiler inputs, but keep the library navigable by responsibility.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files"
    FILES ${AOI_TASKCONTROL_SOURCES}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Header Files"
    FILES ${AOI_TASKCONTROL_HEADERS}
)
source_group("Resources" FILES "${CMAKE_CURRENT_SOURCE_DIR}/TaskControl.rc")

add_library(TaskControl SHARED
    ${AOI_TASKCONTROL_SOURCES}
    ${AOI_TASKCONTROL_HEADERS}
)
add_library(AOI::TaskControl ALIAS TaskControl)

target_compile_definitions(TaskControl PRIVATE TASKCONTROL_EXPORTS QT_NO_KEYWORDS)
target_include_directories(TaskControl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Logger>
)
target_link_libraries(TaskControl
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Eigen3::Eigen
        opencv::opencv
        spdlog::spdlog
        Taskflow::Taskflow
)

set_target_properties(TaskControl PROPERTIES
    DEBUG_POSTFIX d
    FOLDER "Libraries"
    # The legacy DLL exposes its C++ operator registration entry points without
    # per-function export annotations. Preserve the existing DLL ABI here.
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)


if(AOI_BUILD_TASKCONTROL_TESTS)
    find_package(Catch2 CONFIG REQUIRED)

    add_executable(TaskControlTests
        tests/CoreArchitectureTests.cpp
    )
    target_link_libraries(TaskControlTests PRIVATE AOI::TaskControl Catch2::Catch2WithMain)
    add_test(NAME TaskControl.CoreArchitecture COMMAND TaskControlTests)
    set_target_properties(TaskControlTests PROPERTIES FOLDER "Tests")

    # ctest launches this exe straight from the build tree with no Conan shell
    # active, and TaskControl is a SHARED library that links Qt6::Core / Qt6::Gui.
    # Without the DLL closure beside the exe the process dies before main() with
    #   0xC0000135 (STATUS_DLL_NOT_FOUND)
    # because Qt6Cored.dll / Qt6Guid.dll live under the Conan package bin
    # directories, which are not on PATH.  Same treatment Shrimp already gets.
    aoi_deploy_runtime_dependencies(TaskControlTests)
endif()
