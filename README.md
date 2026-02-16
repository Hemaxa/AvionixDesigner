# AvionixDesigner

Визуальный редактор для работы с графическими объектами авионики

## Сборка и запуск

```bash
cd /path/to/AvionixDesigner

mkdir -p build && cd build

cmake ..

cmake --build .

./AvionixDesigner.app/Contents/MacOS/AvionixDesigner
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