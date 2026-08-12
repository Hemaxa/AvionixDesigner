cmake_minimum_required(VERSION 3.19)
project(AvionixDesigner VERSION 1.0.4 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Xml Svg SerialPort)

qt_standard_project_setup()

set(APP_ICON_DIR "${CMAKE_SOURCE_DIR}/res/app-icons")
set(APP_ICON_PNG "${APP_ICON_DIR}/icon.png")
set(APP_ICON_ICO "${APP_ICON_DIR}/AvionixDesigner.ico")
set(APP_ICON_ICNS "${APP_ICON_DIR}/AvionixDesigner.icns")
set(APP_ICON_RC "${APP_ICON_DIR}/AvionixDesigner.rc")

set(APP_PLATFORM_ICON_RESOURCE)
if(WIN32)
    set(APP_PLATFORM_ICON_RESOURCE "${APP_ICON_RC}")
elseif(APPLE)
    set(APP_PLATFORM_ICON_RESOURCE "${APP_ICON_ICNS}")
    set_source_files_properties("${APP_ICON_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()

qt_add_executable(AvionixDesigner
    WIN32 MACOSX_BUNDLE
    ${APP_PLATFORM_ICON_RESOURCE}
    
    # Точка входа
    src/main.cpp
    
    # Базовые классы
    src/BaseObject.h
    src/BasePanel.h

    # Внутренняя модель редактора
    src/editor/EditorProjectDocument.h
    src/editor/EditorProjectDocument.cpp

    # Реестр FPGA-схем
    src/schema/FpgaSchemaRegistry.h
    src/schema/FpgaSchemaRegistry.cpp
    
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
    src/panels/SelectionToolStrip.h
    src/panels/SelectionToolStrip.cpp
    src/panels/EditorWorkspacePanel.h
    src/panels/EditorWorkspacePanel.cpp
    src/panels/FpgaScreenWidget.h
    src/panels/FpgaScreenWidget.cpp
    src/panels/FpgaStreamingPanel.h
    src/panels/FpgaStreamingPanel.cpp
    src/panels/FpgaSimulatorPanel.h
    src/panels/FpgaSimulatorPanel.cpp
    
    # Объекты
    src/objects/RectangleObject.h
    src/objects/RectangleObject.cpp
    src/objects/DashedLineObject.h
    src/objects/DashedLineObject.cpp
    src/objects/RibbonScaleObject.h
    src/objects/RibbonScaleObject.cpp
    src/objects/RotationObject.h
    src/objects/RotationObject.cpp
    src/objects/StaticGroupObject.h
    src/objects/StaticGroupObject.cpp
    src/objects/ImageObject.h
    src/objects/ImageObject.cpp
    src/objects/AviaHorizonObject.h
    src/objects/AviaHorizonObject.cpp
    src/objects/TextObject.h
    src/objects/TextObject.cpp
    
    # FPGA Подсистема
    src/fpga/model/FpgaCompiledDocument.h
    src/fpga/model/FpgaCompiledDocument.cpp
    src/fpga/packets/FpgaPacket.h
    src/fpga/packets/FpgaPacket.cpp
    src/fpga/packets/FpgaPacketCompiler.h
    src/fpga/packets/FpgaPacketCompiler.cpp
    src/fpga/packets/FpgaPacketFramer.h
    src/fpga/packets/FpgaPacketFramer.cpp
    src/fpga/transport/FpgaPacketDump.h
    src/fpga/transport/FpgaPacketDump.cpp
    src/fpga/transport/FpgaUartTransport.h
    src/fpga/transport/FpgaUartTransport.cpp
    src/fpga/simulator/FpgaSimulator.h
    src/fpga/simulator/FpgaSimulator.cpp
    src/fpga/FpgaProjectPacketBuilder.h
    src/fpga/FpgaProjectPacketBuilder.cpp
    
    # Окна
    src/windows/MainWindow.h
    src/windows/MainWindow.cpp
    src/windows/NewProjectDialog.h
    src/windows/NewProjectDialog.cpp
    src/windows/SettingsWindow.h
    src/windows/SettingsWindow.cpp
    src/windows/FontExportDialog.h
    src/windows/FontExportDialog.cpp
    
    # Утилиты (header-only)
    src/utils/BitParser.h
    src/utils/XmlReader.h
    src/utils/ProportionalResize.h
    
    # Ресурсы
    res/resources.qrc
)

target_link_libraries(AvionixDesigner
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Xml
        Qt::Svg
        Qt::SerialPort
)

target_compile_features(AvionixDesigner PRIVATE cxx_std_17)

target_compile_definitions(AvionixDesigner PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

# Добавляем директории для include, чтобы не использовать относительные пути
target_include_directories(AvionixDesigner PRIVATE
    ${APP_ICON_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/managers
    ${CMAKE_SOURCE_DIR}/src/panels
    ${CMAKE_SOURCE_DIR}/src/objects
    ${CMAKE_SOURCE_DIR}/src/editor
    ${CMAKE_SOURCE_DIR}/src/schema
    ${CMAKE_SOURCE_DIR}/src/windows
    ${CMAKE_SOURCE_DIR}/src/utils
)

set_target_properties(AvionixDesigner PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    MACOSX_BUNDLE_BUNDLE_NAME "Avionix Designer"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.avionix.designer"
    MACOSX_BUNDLE_ICON_FILE "AvionixDesigner.icns"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
)

file(COPY ${CMAKE_SOURCE_DIR}/res DESTINATION ${CMAKE_BINARY_DIR})

include(GNUInstallDirs)

install(TARGETS AvionixDesigner
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION .)

qt_generate_deploy_app_script(
    TARGET AvionixDesigner
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

set(CPACK_PACKAGE_NAME "AvionixDesigner")
set(CPACK_PACKAGE_VENDOR "Avionix")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "AvionixDesigner")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Avionix Designer interface editor")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_FILE_NAME "AvionixDesigner-${PROJECT_VERSION}-windows-x64")
    set(CPACK_NSIS_DISPLAY_NAME "Avionix Designer")
    set(CPACK_NSIS_PACKAGE_NAME "Avionix Designer")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_MUI_ICON "${APP_ICON_ICO}")
    set(CPACK_NSIS_MUI_UNIICON "${APP_ICON_ICO}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\AvionixDesigner.exe")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA [[
  StrCmp "$INSTALL_DESKTOP" "1" 0 +2
    CreateShortCut "$DESKTOP\Avionix Designer.lnk" "$INSTDIR\bin\AvionixDesigner.exe"
    ]])
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [[
  Delete "$DESKTOP\Avionix Designer.lnk"
    ]])
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGE_FILE_NAME "AvionixDesigner-${PROJECT_VERSION}-macos")
    set(CPACK_DMG_VOLUME_NAME "Avionix Designer")
    set(CPACK_DMG_FORMAT "UDZO")
    set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE ON)
else()
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "AvionixDesigner-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
endif()
include(CPack)
