set(AOI_RUNTIME_SOURCES
    RuntimeScheduler.cpp
)

set(AOI_RUNTIME_HEADERS
    RuntimeScheduler.h
)

find_package(Taskflow CONFIG REQUIRED)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${AOI_RUNTIME_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Header Files" FILES ${AOI_RUNTIME_HEADERS})

add_library(AOIRuntime STATIC ${AOI_RUNTIME_SOURCES} ${AOI_RUNTIME_HEADERS})
add_library(AOI::Runtime ALIAS AOIRuntime)

target_include_directories(AOIRuntime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(AOIRuntime PUBLIC AOI::TaskControl Taskflow::Taskflow)
target_compile_definitions(AOIRuntime PRIVATE QT_NO_KEYWORDS)
set_target_properties(AOIRuntime PROPERTIES FOLDER "Libraries")

if(AOI_BUILD_TASKCONTROL_TESTS)
    find_package(Catch2 CONFIG REQUIRED)
    add_executable(RuntimeTests tests/RuntimeSchedulerTests.cpp)
    target_link_libraries(RuntimeTests PRIVATE AOI::Runtime Catch2::Catch2WithMain)
    add_test(NAME Runtime.Scheduler COMMAND RuntimeTests)
    set_target_properties(RuntimeTests PROPERTIES FOLDER "Tests")
endif()
