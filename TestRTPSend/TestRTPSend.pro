QT       += core gui network multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ffmpegheader.cpp \
    main.cpp \
    mediacapturer.cpp \
    mediaoutput.cpp \
    rtp.cpp \
    rtpsend.cpp \
    widget.cpp \
    worker.cpp

HEADERS += \
    ffmpegheader.h \
    mediacapturer.h \
    mediaoutput.h \
    rtp.h \
    rtpsend.h \
    widget.h \
    worker.h

FORMS += \
    widget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32: LIBS += -L$$PWD/FFmpegLib/lib/ -lavcodec
win32: LIBS += -L$$PWD/FFmpegLib/lib/ -lavdevice
win32: LIBS += -L$$PWD/FFmpegLib/lib/ -lavfilter
win32: LIBS += -L$$PWD/FFmpegLib/lib/ -lavformat
win32: LIBS += -L$$PWD/FFmpegLib/lib/ -lavutil
win32: LIBS += -L$$PWD/FFmpegLib/lib/ -lpostproc
win32: LIBS += -L$$PWD/FFmpegLib/lib/ -lswresample
win32: LIBS += -L$$PWD/FFmpegLib/lib/ -lswscale
INCLUDEPATH += $$PWD/FFmpegLib/include
DEPENDPATH += $$PWD/FFmpegLib/include

win32: LIBS += -L$$PWD/SDLLib/lib/ -lSDL2
INCLUDEPATH += $$PWD/SDLLib/include
DEPENDPATH += $$PWD/SDLLib/include
