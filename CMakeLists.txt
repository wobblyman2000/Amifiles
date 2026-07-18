cmake_minimum_required(VERSION 3.16)
project(Amifiles VERSION 1.3.6 LANGUAGES CXX)

configure_file(
    "${PROJECT_SOURCE_DIR}/src/version.h.in"
    "${PROJECT_BINARY_DIR}/src/version.h"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 projects require these automatic compilation tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets Svg Network)

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
    src/spaceanalyzer.h
    src/spaceanalyzer.cpp
    src/diffdialog.h
    src/diffdialog.cpp
    src/tageditordialog.h
    src/tageditordialog.cpp
    src/metadatafetcherdialog.h
    src/metadatafetcherdialog.cpp
    src/videoscraperdialog.h
    src/videoscraperdialog.cpp
    src/terminalpanel.h
    src/terminalpanel.cpp
    src/keybindingseditor.h
    src/keybindingseditor.cpp
    src/checksumdialog.h
    src/checksumdialog.cpp
    src/shreddialog.h
    src/shreddialog.cpp
    src/remotemountdialog.h
    src/remotemountdialog.cpp
    src/imageconverterdialog.h
    src/imageconverterdialog.cpp
    src/tagmanager.h
    src/tagmanager.cpp
    src/processmanagerdialog.h
    src/processmanagerdialog.cpp
    src/vaultdialog.h
    src/vaultdialog.cpp
    src/sunburstchart.h
    src/sunburstchart.cpp
    src/archivedialog.h
    src/archivedialog.cpp
    src/imageeditordialog.h
    src/imageeditordialog.cpp
    src/hexeditorwidget.h
    src/hexeditorwidget.cpp
    src/cloudmountdialog.h
    src/cloudmountdialog.cpp
    src/pdfviewerwidget.h
    src/pdfviewerwidget.cpp
    src/agestylingdialog.h
    src/agestylingdialog.cpp
    src/preferencesdialog.h
    src/preferencesdialog.cpp
    src/folderlayoutdialog.h
    src/folderlayoutdialog.cpp
    src/dynamicfavoritesdialog.h
    src/dynamicfavoritesdialog.cpp
    src/treemapchartwidget.h
    src/treemapchartwidget.cpp
    src/syncscheduler.h
    src/syncscheduler.cpp
    src/schedulerdialog.h
    src/schedulerdialog.cpp
    src/fullscreenimageviewer.h
    src/fullscreenimageviewer.cpp
    src/autotagdialog.h
    src/autotagdialog.cpp
    src/cardviewdelegate.h
    src/cardviewdelegate.cpp
    src/millercolumnsview.h
    src/millercolumnsview.cpp
    src/timelineview.h
    src/timelineview.cpp
    src/filmstripview.h
    src/filmstripview.cpp
    src/commandpalette.h
    src/commandpalette.cpp
    src/folderdiffdialog.h
    src/folderdiffdialog.cpp
    src/smartfoldermodel.h
    src/smartfoldermodel.cpp
    src/filetagsdialog.h
    src/filetagsdialog.cpp
    src/workspaceprofiledialog.h
    src/workspaceprofiledialog.cpp
    src/diskdashboardwidget.h
    src/diskdashboardwidget.cpp
    src/autoorganizerdialog.h
    src/autoorganizerdialog.cpp
    src/remotemountmanager.h
    src/remotemountmanager.cpp
    src/custommenueditordialog.h
    src/custommenueditordialog.cpp
    src/iconpickerdialog.h
    src/iconpickerdialog.cpp
    src/toolbareditordialog.h
    src/toolbareditordialog.cpp
    src/actionhelperdialog.h
    src/actionhelperdialog.cpp
    src/texteditordialog.h
    src/texteditordialog.cpp
    src/themestudiodialog.h
    src/themestudiodialog.cpp
    src/smartcollectiondialog.h
    src/smartcollectiondialog.cpp
    src/columnscustomizerdialog.h
    src/columnscustomizerdialog.cpp
    src/groupproxymodel.h
    src/groupproxymodel.cpp
)

target_link_libraries(Amifiles PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Svg
    Qt6::Network
    util
)

target_include_directories(Amifiles PRIVATE
    "${PROJECT_BINARY_DIR}/src"
)

install(TARGETS Amifiles DESTINATION bin)
