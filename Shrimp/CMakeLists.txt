# Shrimp is opt-in because it needs the private VTK package.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network OpenGLWidgets)
find_package(OpenCV CONFIG REQUIRED)
find_package(PCL CONFIG REQUIRED COMPONENTS common io)
find_package(spdlog CONFIG REQUIRED)
find_package(VTK CONFIG REQUIRED COMPONENTS
    CommonCore CommonDataModel CommonExecutionModel CommonMath CommonTransforms
    FiltersCore FiltersGeneral FiltersSources InteractionStyle InteractionWidgets
    RenderingAnnotation RenderingCore RenderingFreeType RenderingOpenGL2 RenderingUI
    GUISupportQt RenderingQt
)
file(GLOB_RECURSE AOI_SHRIMP_CPP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
file(GLOB_RECURSE AOI_SHRIMP_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
file(GLOB_RECURSE AOI_SHRIMP_FORMS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ui"
)
file(GLOB_RECURSE AOI_SHRIMP_RESOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.qrc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.rc"
)
file(GLOB_RECURSE AOI_SHRIMP_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ico"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.qss"
)

set(AOI_SHRIMP_SOURCES
    ${AOI_SHRIMP_CPP_SOURCES}
    ${AOI_SHRIMP_HEADERS}
    ${AOI_SHRIMP_FORMS}
    ${AOI_SHRIMP_RESOURCES}
    ${AOI_SHRIMP_ASSETS}
)

# AlgorithmSDK keeps its operator registry as a header-defined singleton.  The
# GUI constructs AlgorithmTask instances in this executable, so its built-in
# operator registrations must live in this module as well as TaskControl.dll.
list(APPEND AOI_SHRIMP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../TaskControl/operators/RegisterBuiltinOperators.cpp"
)

# Logical filters for the Visual Studio CMake project.  This mirrors the
# source tree, separates forms and assets from code, and makes each feature
# area (System, UserManager, GUICommon, Logger, ...) directly discoverable.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files"
    FILES ${AOI_SHRIMP_CPP_SOURCES}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Header Files"
    FILES ${AOI_SHRIMP_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Forms"
    FILES ${AOI_SHRIMP_FORMS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Resources"
    FILES ${AOI_SHRIMP_RESOURCES} ${AOI_SHRIMP_ASSETS}
)
source_group("Dependencies/TaskControl" FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../TaskControl/operators/RegisterBuiltinOperators.cpp"
)

# The legacy project uses unqualified includes across its feature folders.
# Preserve that layout without relying on IDE-specific include settings.
set(AOI_SHRIMP_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(AOI_SHRIMP_HEADER IN LISTS AOI_SHRIMP_HEADERS)
    get_filename_component(AOI_SHRIMP_HEADER_DIR "${AOI_SHRIMP_HEADER}" DIRECTORY)
    list(APPEND AOI_SHRIMP_INCLUDE_DIRS "${AOI_SHRIMP_HEADER_DIR}")
endforeach()
list(REMOVE_DUPLICATES AOI_SHRIMP_INCLUDE_DIRS)

add_executable(Shrimp WIN32 ${AOI_SHRIMP_SOURCES})
set_target_properties(Shrimp PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    FOLDER "Applications"
)

# Qt 6 removed QSettings::setIniCodec because INI files are UTF-8.  Restrict
# the compatibility type to the two legacy translation units that use it.
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/System/systemsetting.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/UserManager/usermanager.cpp"
    PROPERTIES COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/Qt6QSettingsCompat.h"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/pointcloudwidget.cpp"
    PROPERTIES COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/TaskflowQtKeywordCompat.h"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/../TaskControl/operators/RegisterBuiltinOperators.cpp"
    PROPERTIES COMPILE_DEFINITIONS QT_NO_KEYWORDS
)
target_include_directories(Shrimp PRIVATE ${AOI_SHRIMP_INCLUDE_DIRS})
target_link_libraries(Shrimp
    PRIVATE
        AOI::TaskControl
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::OpenGLWidgets
        opencv::opencv
        PCL::PCL
        VTK::CommonCore VTK::CommonDataModel VTK::CommonExecutionModel VTK::CommonMath
        VTK::CommonTransforms VTK::FiltersCore VTK::FiltersGeneral VTK::FiltersSources
        VTK::InteractionStyle VTK::InteractionWidgets VTK::RenderingAnnotation
        VTK::RenderingCore VTK::RenderingFreeType VTK::RenderingOpenGL2
        VTK::RenderingUI VTK::GUISupportQt VTK::RenderingQt
        spdlog::spdlog
)

if(COMMAND vtk_module_autoinit)
    vtk_module_autoinit(TARGETS Shrimp MODULES ${VTK_LIBRARIES})
endif()

# Deploy TaskControl, VTK, OpenCV, PCL and their shared dependencies beside
# Shrimp, then deploy Qt's required runtime plugins.
aoi_deploy_runtime_dependencies(Shrimp)
