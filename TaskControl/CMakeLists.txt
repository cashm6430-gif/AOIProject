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
