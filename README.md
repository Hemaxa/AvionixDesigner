# AvionixDesigner

Визуальный редактор для работы с графическими объектами авионики.

## Описание

AvionixDesigner — это Qt6-приложение для загрузки, отображения и редактирования графических объектов, описанных в XML-формате. Приложение парсит объекты (прямоугольники, объекты вращения, статические группы) из HEX-строк по заданной битовой схеме параметров.

### Основные компоненты

- **Менеджеры**: `ProjectManager`, `ObjectsManager`, `AppearanceManager`, `PanelsManager`
- **Панели**: `ViewportPanel`, `ObjectListPanel`, `ObjectPropertiesPanel`, `ObjectLibraryPanel`
- **Объекты**: `RectangleObject`, `RotationObject`, `StaticGroupObject`

## Требования

- Qt 6.5+
- CMake 3.19+
- Компилятор с поддержкой C++17

## Сборка и запуск

```bash
# Перейти в директорию проекта
cd /path/to/AvionixDesigner

# Создать директорию сборки и перейти в неё
mkdir -p build && cd build

# Сконфигурировать проект
cmake ..

# Собрать проект
cmake --build .

# Запустить приложение (macOS)
./AvionixDesigner.app/Contents/MacOS/AvionixDesigner
# или на Linux/Windows
./AvionixDesigner
```

## Структура проекта

```
AvionixDesigner/
├── CMakeLists.txt          # Конфигурация сборки
├── src/
│   ├── main.cpp            # Точка входа
│   ├── BaseObject.h        # Базовый класс объектов
│   ├── BasePanel.h         # Базовый класс панелей
│   ├── managers/           # Менеджеры (синглтоны)
│   ├── objects/            # Графические объекты
│   ├── panels/             # UI-панели
│   ├── windows/            # Окна приложения
│   └── utils/              # Утилиты (BitParser, XmlReader)
└── res/                    # Ресурсы (XML-файлы, темы)
```