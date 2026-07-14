cmake_minimum_required(VERSION 3.16)
project(Amifiles VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 projects require these automatic compilation tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets Svg)

add_executable(Amifiles
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/filepanel.h
    src/filepanel.cpp
    src/previewpanel.h
    src/previewpanel.cpp
    src/favoritesmanager.h
    src/favoritesmanager.cpp
    src/metadataextractor.h
    src/metadataextractor.cpp
    src/bulkrename.h
    src/bulkrename.cpp
    src/theme.h
    src/consolepanel.h
    src/consolepanel.cpp
    src/foldersizecalculator.h
    src/foldersizecalculator.cpp
    src/flatmodel.h
    src/flatmodel.cpp
    src/foldersync.h
    src/foldersync.cpp
    src/dupfinder.h
    src/dupfinder.cpp
    src/copyqueue.h
    src/copyqueue.cpp
    src/archivemodel.h
    src/archivemodel.cpp
    src/customfilesystemmodel.h
    src/customfilesystemmodel.cpp
    src/buttoneditor.h
    src/buttoneditor.cpp
    src/searchworker.h
    src/searchworker.cpp
    src/helpdialog.h
    src/helpdialog.cpp
)

target_link_libraries(Amifiles PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Svg
)

install(TARGETS Amifiles DESTINATION bin)
