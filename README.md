# AvionixDesigner

AvionixDesigner - это редактор кадров для графического контроллера на базе ПЛИС.

## Скриншот

<img width="1840" height="1191" alt="Снимок экрана 2026-07-27 в 02 46 15" src="https://github.com/user-attachments/assets/55a007be-ea3c-49ce-90be-c08732e55da8" />

## Сборка

Требования:

- CMake 3.19 или новее.
- Компилятор C++17.
- Qt 6.5 или новее с модулями `Core`, `Widgets`, `Xml`, `Svg`.
- NSIS нужен только для сборки Windows-инсталлятора через CPack.

Локальная сборка:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

## Файловая Структура

```text
.
|-- .github/
|-- .gitignore
|-- CMakeLists.txt
|-- README.md
|-- docs/
|-- res/
|-- src/
|   |-- BaseObject.h
|   |-- BasePanel.h
|   |-- editor/
|   |   |-- EditorProjectDocument.cpp
|   |   `-- EditorProjectDocument.h
|   |-- main.cpp
|   |-- managers/
|   |   |-- AppearanceManager.cpp
|   |   |-- AppearanceManager.h
|   |   |-- ObjectsManager.cpp
|   |   |-- ObjectsManager.h
|   |   |-- PanelsManager.cpp
|   |   |-- PanelsManager.h
|   |   |-- ProjectManager.cpp
|   |   `-- ProjectManager.h
|   |-- objects/
|   |   |-- AviaHorizonObject.cpp
|   |   |-- AviaHorizonObject.h
|   |   |-- DashedLineObject.cpp
|   |   |-- DashedLineObject.h
|   |   |-- ImageObject.cpp
|   |   |-- ImageObject.h
|   |   |-- RectangleObject.cpp
|   |   |-- RectangleObject.h
|   |   |-- RibbonScaleObject.cpp
|   |   |-- RibbonScaleObject.h
|   |   |-- RotationObject.cpp
|   |   |-- RotationObject.h
|   |   |-- StaticGroupObject.cpp
|   |   |-- StaticGroupObject.h
|   |   |-- TextObject.cpp
|   |   `-- TextObject.h
|   |-- panels/
|   |   |-- EditorWorkspacePanel.cpp
|   |   |-- EditorWorkspacePanel.h
|   |   |-- ObjectLibraryPanel.cpp
|   |   |-- ObjectLibraryPanel.h
|   |   |-- ObjectListPanel.cpp
|   |   |-- ObjectListPanel.h
|   |   |-- ObjectPropertiesPanel.cpp
|   |   |-- ObjectPropertiesPanel.h
|   |   |-- SelectionToolStrip.cpp
|   |   |-- SelectionToolStrip.h
|   |   |-- ViewportPanel.cpp
|   |   |-- ViewportPanel.h
|   |   |-- ViewportSettingsPanel.cpp
|   |   `-- ViewportSettingsPanel.h
|   |-- schema/
|   |   |-- FpgaSchemaRegistry.cpp
|   |   `-- FpgaSchemaRegistry.h
|   |-- utils/
|   |   |-- BitParser.h
|   |   |-- ProportionalResize.h
|   |   `-- XmlReader.h
|   `-- windows/
|       |-- FontExportDialog.cpp
|       |-- FontExportDialog.h
|       |-- MainWindow.cpp
|       |-- MainWindow.h
|       |-- NewProjectDialog.cpp
|       |-- NewProjectDialog.h
|       |-- SettingsWindow.cpp
|       `-- SettingsWindow.h
`-- tests/
    |-- image.xml
    `-- test.xml
```
