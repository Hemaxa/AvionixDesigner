cmake_minimum_required(VERSION 3.19)
project(XML-Editor LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Xml)

qt_standard_project_setup()

qt_add_executable(XML-Editor
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/shapes.h
    src/shapes.cpp
)

# Линкуем библиотеку Xml
target_link_libraries(XML-Editor
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Xml
)

include(GNUInstallDirs)

install(TARGETS XML-Editor
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET XML-Editor
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})