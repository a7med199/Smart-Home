cmake_minimum_required(VERSION 3.16)
project(SmartHome VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

set(HEADERS
    include/Device.h
    include/Room.h
    include/SmartHome.h
    include/Models.h
    include/Widgets.h
    include/Pages.h
    include/MainWindow.h
)

set(SOURCES
    src/main.cpp
    src/Device.cpp
    src/Room.cpp
    src/SmartHome.cpp
    src/Widgets.cpp
    src/Pages.cpp
    src/MainWindow.cpp
)

include_directories(include)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(SmartHome MANUAL_FINALIZATION ${SOURCES} ${HEADERS})
else()
    add_executable(SmartHome ${SOURCES} ${HEADERS})
endif()

target_link_libraries(SmartHome PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

set_target_properties(SmartHome PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(SmartHome)
endif()
