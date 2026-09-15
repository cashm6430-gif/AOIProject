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

add_library(TaskControl SHARED ${AOI_TASKCONTROL_SOURCES})
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
    # The legacy DLL exposes its C++ operator registration entry points without
    # per-function export annotations. Preserve the existing DLL ABI here.
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
