OPENCV_POSTFIX = 452
OPENCV = D:/PublicLibraries/opencv/opencv-4.5.2

INCLUDEPATH += \
            $$OPENCV/build/include \

win32:CONFIG(debug, debug|release):{
    LIBS += -L$$OPENCV/build/x64/vc15/lib -lopencv_world$${OPENCV_POSTFIX}d
}
else:win32:CONFIG(release, debug|release):{
    LIBS += -L$$OPENCV/build/x64/vc15/lib -lopencv_world$${OPENCV_POSTFIX}
}

