cmake_minimum_required(VERSION 3.19)
project(XML-Editor LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Xml)

qt_standard_project_setup()

qt_add_executable(XML-Editor
    WIN32 MACOSX_BUNDLE
    
    # Точка входа
    src/main.cpp
    
    # Менеджеры
    src/managers/ProjectManager.h
    src/managers/ProjectManager.cpp
    src/managers/ObjectsManager.h
    src/managers/ObjectsManager.cpp
    src/managers/AppearanceManager.h
    src/managers/AppearanceManager.cpp
    
    # Объекты
    src/objects/AbstractObject.h
    src/objects/RectangleObject.h
    src/objects/RectangleObject.cpp
    src/objects/RotationObject.h
    src/objects/RotationObject.cpp
    src/objects/StaticGroupObject.h
    src/objects/StaticGroupObject.cpp
    
    # Окна
    src/windows/MainWindow.h
    src/windows/MainWindow.cpp
    src/windows/ViewportWindow.h
    src/windows/ViewportWindow.cpp
    src/windows/ObjectsListWindow.h
    src/windows/ObjectsListWindow.cpp
    src/windows/PropertiesWindow.h
    src/windows/PropertiesWindow.cpp
    src/windows/LogWindow.h
    src/windows/LogWindow.cpp
    
    # Утилиты (header-only)
    src/utils/BitParser.h
    src/utils/XmlHelper.h
)

target_link_libraries(XML-Editor
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Xml
)

target_compile_features(XML-Editor PRIVATE cxx_std_17)

set_target_properties(XML-Editor PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

file(COPY ${CMAKE_SOURCE_DIR}/res DESTINATION ${CMAKE_BINARY_DIR})

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