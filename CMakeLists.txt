cmake_minimum_required(VERSION 3.19)
project(AvionixDesigner LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Xml)

qt_standard_project_setup()

qt_add_executable(AvionixDesigner
    WIN32 MACOSX_BUNDLE
    
    # Точка входа
    src/main.cpp
    
    # Базовые классы
    src/BaseObject.h
    src/BasePanel.h
    
    # Менеджеры
    src/managers/ProjectManager.h
    src/managers/ProjectManager.cpp
    src/managers/ObjectsManager.h
    src/managers/ObjectsManager.cpp
    src/managers/AppearanceManager.h
    src/managers/AppearanceManager.cpp
    src/managers/PanelsManager.h
    src/managers/PanelsManager.cpp
    
    # Панели
    src/panels/ViewportPanel.h
    src/panels/ViewportPanel.cpp
    src/panels/ObjectListPanel.h
    src/panels/ObjectListPanel.cpp
    src/panels/ObjectPropertiesPanel.h
    src/panels/ObjectPropertiesPanel.cpp
    src/panels/ObjectLibraryPanel.h
    src/panels/ObjectLibraryPanel.cpp
    src/panels/ViewportSettingsPanel.h
    src/panels/ViewportSettingsPanel.cpp
    
    # Объекты
    src/objects/RectangleObject.h
    src/objects/RectangleObject.cpp
    src/objects/RotationObject.h
    src/objects/RotationObject.cpp
    src/objects/StaticGroupObject.h
    src/objects/StaticGroupObject.cpp
    src/objects/AviaHorizonObject.h
    src/objects/AviaHorizonObject.cpp
    src/objects/TextObject.h
    src/objects/TextObject.cpp
    
    # Окна
    src/windows/MainWindow.h
    src/windows/MainWindow.cpp
    src/windows/NewProjectDialog.h
    src/windows/NewProjectDialog.cpp
    src/windows/SettingsWindow.h
    src/windows/SettingsWindow.cpp
    
    # Утилиты (header-only)
    src/utils/BitParser.h
    src/utils/XmlReader.h
    src/utils/DebugDumper.h
    
    # Ресурсы
    res/resources.qrc
)

target_link_libraries(AvionixDesigner
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Xml
)

target_compile_features(AvionixDesigner PRIVATE cxx_std_17)

# Добавляем директории для include, чтобы не использовать относительные пути
target_include_directories(AvionixDesigner PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/managers
    ${CMAKE_SOURCE_DIR}/src/panels
    ${CMAKE_SOURCE_DIR}/src/objects
    ${CMAKE_SOURCE_DIR}/src/windows
    ${CMAKE_SOURCE_DIR}/src/utils
)

set_target_properties(AvionixDesigner PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

file(COPY ${CMAKE_SOURCE_DIR}/res DESTINATION ${CMAKE_BINARY_DIR})

include(GNUInstallDirs)

install(TARGETS AvionixDesigner
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET AvionixDesigner
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
