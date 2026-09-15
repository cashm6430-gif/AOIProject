QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

RC_FILE = Shrimp_resource.rc
RC_ICONS = logo.ico

TEMPLATE = app

CONFIG += c++20

QMAKE_CXXFLAGS += /std:c++20
QMAKE_CXXFLAGS += -execution-charset:utf-8
QMAKE_CXXFLAGS += -source-charset:utf-8
QMAKE_CXXFLAGS += /arch:AVX2

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

BUILD = $$PWD/../build

include(./opencv.pri)
include(./pcl&vtk.pri)
include(./GUICommon/GUICommon.pri)
include(./Logger/Logger.pri)
include(./System/System.pri)
include(./UserManager/UserManager.pri)

SOURCES += \
    ImageWidget.cpp \
    PclVtkConverter.cpp \
    VtkCloudRenderer.cpp \
    algcontrol.cpp \
    algrecipe.cpp \
    main.cpp \
    mainwindow.cpp \
    pointcloudwidget.cpp \
    qimageview.cpp \
    roiitem.cpp

HEADERS += \
    ImageWidget.h \
    PclVtkConverter.h \
    VtkCloudRenderer.h \
    algcontrol.h \
    algrecipe.h \
    datadefine.h \
    mainwindow.h \
    pointcloudwidget.h \
    qimageview.h \
    roiitem.h

FORMS += \
    ImageWidget.ui \
    mainwindow.ui \
    pointcloudwidget.ui

INCLUDEPATH += $$PWD \
    $$PWD/../thirdparty/open3d-0.19.0/include \
    $$PWD/../thirdparty/vcpkg/include \
    $$PWD/../HAL/Windump \
    $$PWD/../build/include

win32:CONFIG(debug, debug|release):{
    DESTDIR = $$BUILD/bin/debug
    LIBS += -L$$DESTDIR -lWindump
    LIBS += -L$$PWD/../thirdparty/open3d-0.19.0/lib/debug -lOpen3D
    LIBS += -L$$PWD/../thirdparty/vcpkg/debug/lib -lfmtd
    LIBS += -L$$PWD/../thirdparty/vcpkg/debug/lib -lspdlogd
    LIBS += -L$$PWD/../build/lib/Debug -lTaskControl
}
else:win32:CONFIG(release, debug|release):{
    DESTDIR = $$BUILD/bin/release
    LIBS += -L$$DESTDIR -lWindump
    LIBS += -L$$PWD/../thirdparty/open3d-0.19.0/lib/release -lOpen3D
    LIBS += -L$$PWD/../thirdparty/vcpkg/lib -lfmt
    LIBS += -L$$PWD/../thirdparty/vcpkg/lib -lspdlog
    LIBS += -L$$PWD/../build/lib/Release -lTaskControl
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource.qrc
