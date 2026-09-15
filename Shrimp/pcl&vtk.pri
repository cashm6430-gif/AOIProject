OPENCV_POSTFIX = 452
PCL = D:/PublicLibraries/PCL_1.15.1
VTK = D:/PublicLibraries/vtk942_qt512

INCLUDEPATH += \
    $$PCL/include/pcl-1.15 \
    $$PCL/3rdParty/Boost/include/boost-1_87 \
    $$PCL/3rdParty/Eigen3/include/eigen3 \
    $$VTK/include/vtk-9.4

win32:CONFIG(debug, debug|release):{
    LIBS += -L$$PCL/lib -lpcl_commond
    LIBS += -L$$PCL/lib -lpcl_iod
    LIBS += -L$$PCL/lib -lpcl_io_plyd
    LIBS += -L$$VTK/lib -lvtkCommonCore-9.4-gd
    LIBS += -L$$VTK/lib -lvtkCommonDataModel-9.4-gd
    LIBS += -L$$VTK/lib -lvtkCommonExecutionModel-9.4-gd
    LIBS += -L$$VTK/lib -lvtkCommonMath-9.4-gd
    LIBS += -L$$VTK/lib -lvtkCommonTransforms-9.4-gd
    LIBS += -L$$VTK/lib -lvtkFiltersCore-9.4-gd
    LIBS += -L$$VTK/lib -lvtkFiltersGeneral-9.4-gd
    LIBS += -L$$VTK/lib -lvtkFiltersSources-9.4-gd
    LIBS += -L$$VTK/lib -lvtkInteractionStyle-9.4-gd
    LIBS += -L$$VTK/lib -lvtkInteractionWidgets-9.4-gd
    LIBS += -L$$VTK/lib -lvtkRenderingAnnotation-9.4-gd
    LIBS += -L$$VTK/lib -lvtkRenderingCore-9.4-gd
    LIBS += -L$$VTK/lib -lvtkRenderingFreeType-9.4-gd
    LIBS += -L$$VTK/lib -lvtkRenderingOpenGL2-9.4-gd
    LIBS += -L$$VTK/lib -lvtkRenderingUI-9.4-gd
    LIBS += -L$$VTK/lib -lvtkGUISupportQt-9.4-gd
    LIBS += -L$$VTK/lib -lvtkRenderingQt-9.4-gd
    LIBS += -L$$VTK/lib -lvtksys-9.4-gd
}
else:win32:CONFIG(release, debug|release):{
    LIBS += -L$$PCL/lib -lpcl_common
    LIBS += -L$$PCL/lib -lpcl_io
    LIBS += -L$$PCL/lib -lpcl_io_ply
    LIBS += -L$$VTK/lib -lvtkCommonCore-9.4
    LIBS += -L$$VTK/lib -lvtkCommonDataModel-9.4
    LIBS += -L$$VTK/lib -lvtkCommonExecutionModel-9.4
    LIBS += -L$$VTK/lib -lvtkCommonMath-9.4
    LIBS += -L$$VTK/lib -lvtkCommonTransforms-9.4
    LIBS += -L$$VTK/lib -lvtkFiltersCore-9.4
    LIBS += -L$$VTK/lib -lvtkFiltersGeneral-9.4
    LIBS += -L$$VTK/lib -lvtkFiltersSources-9.4
    LIBS += -L$$VTK/lib -lvtkInteractionStyle-9.4
    LIBS += -L$$VTK/lib -lvtkInteractionWidgets-9.4
    LIBS += -L$$VTK/lib -lvtkRenderingAnnotation-9.4
    LIBS += -L$$VTK/lib -lvtkRenderingCore-9.4
    LIBS += -L$$VTK/lib -lvtkRenderingFreeType-9.4
    LIBS += -L$$VTK/lib -lvtkRenderingOpenGL2-9.4
    LIBS += -L$$VTK/lib -lvtkRenderingUI-9.4
    LIBS += -L$$VTK/lib -lvtkGUISupportQt-9.4
    LIBS += -L$$VTK/lib -lvtkRenderingQt-9.4
    LIBS += -L$$VTK/lib -lvtksys-9.4
}

