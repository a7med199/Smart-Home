QT       += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET   = SmartHome
TEMPLATE = app

CONFIG  += c++17

# Suppress deprecation warnings for Qt < 6
DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += include

HEADERS += \
    include/Device.h       \
    include/Room.h         \
    include/SmartHome.h    \
    include/Models.h       \
    include/Widgets.h      \
    include/Pages.h        \
    include/MainWindow.h

SOURCES += \
    src/main.cpp           \
    src/Device.cpp         \
    src/Room.cpp           \
    src/SmartHome.cpp      \
    src/Widgets.cpp        \
    src/Pages.cpp          \
    src/MainWindow.cpp

# Default rules for deployment
qnx:    target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
