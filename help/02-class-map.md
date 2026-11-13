# 02. Карта классов и файлов

[К оглавлению](README.md) · [Далее: объекты](03-objects-and-resources.md)

В таблицах указан путь относительно корня репозитория. Если есть одноимённые `.h` и `.cpp`, интерфейс находится в заголовке, реализация — в `.cpp`.

## Основа и управление проектом

| Класс | Файл | Ответственность |
| --- | --- | --- |
| `BaseObject` | `src/BaseObject.h` | Общий контракт объекта: рисование, геометрия, свойства, клонирование, аппаратная сериализация и редакторские флаги |
| `BasePanel` | `src/BasePanel.h` | Базовый виджет панели, политика размеров и имя для QSS |
| `ProjectManager` | `src/managers/ProjectManager.*` | Текущая сцена, операции над ней, слои, группы, история, буфер обмена и координация сохранения |
| `ObjectsManager` | `src/managers/ObjectsManager.*` | Фабрика: имя типа → функция создания объекта |
| `AppearanceManager` | `src/managers/AppearanceManager.*` | Загрузка и применение темы приложения |
| `FpgaSchemaRegistry` | `src/schema/FpgaSchemaRegistry.*` | Поля схем в аппаратном порядке, валидация XML и каталог доступных типов |

## Модель редактора

| Компонент | Файл | Ответственность |
| --- | --- | --- |
| `EditorProjectDocument` | `src/editor/EditorProjectDocument.*` | Имя проекта, размер холста и фон |
| `EditorViewState` | `src/editor/EditorViewState.h` | Сохраняемые масштаб, смещение и режимы преобразования |
| `EditorProjectXml::State` | `src/editor/EditorProjectXml.h` | Временное полное состояние для чтения/записи редакторской части XML |
| `EditorProjectXml` | `src/editor/EditorProjectXml.*` | Чтение/запись `<editor>`, связей, ресурсов и метаданных; локальные `Fields` и `Resources` проверяют поля и ресурсы |
| `ProjectXmlFile` | `src/editor/ProjectXmlFile.*` | Файловый ввод/вывод XML и кодировки |
| `ObjectCloner` | `src/editor/ObjectCloner.*` | Копия сцены с перенаправлением ссылок зависимых объектов |
| `SymmetryAxis`, `SymmetryCopyObject` | `src/editor/SymmetryCopyObject.*` | Общая ось и зависимое зеркальное представление исходника |
| Функции отражения | `src/editor/ObjectReflection.*` | Превращение отражения в геометрию независимого обычного объекта |
| `CanvasGeometry` | `src/editor/CanvasGeometry.*` | Общие ограничения перемещения по границам холста |
| `EqualSpacing`, `SpacingGuide` | `src/editor/EqualSpacing.*` | Поиск равных промежутков и геометрия размерных стрелок |
| `ObjectSnapper` | `src/editor/ObjectSnapper.*` | Выбор ближайшей привязки при перемещении и проверка подсказок |

## Графические объекты

Все перечисленные файлы находятся в `src/objects`.

| Класс | Назначение |
| --- | --- |
| `RectangleObject` | Прямоугольник с заливкой, внешней обводкой и прозрачностью; экспорт `rectangle`/`rectangle_a` |
| `DashedLineObject` | Линия по двум концам, толщина, период, длина и фаза штрихов; XML-тип `dashline` |
| `RibbonScaleObject` | Ленточная шкала; XML-имя `RibonScale` сохраняет аппаратное написание |
| `AviaHorizonObject` | Небо, земля и линия горизонта, параметры положения и поворота |
| `RotationObject`, `RotationObjectLayer` | Аппаратный вращающийся объект, его маска и набор init-состояний |
| `StaticGroupObject`, `GroupState` | Несколько состояний/прямоугольников с адресами в общей статической маске |
| `ImageObject`, `ImageColorLayer`, `ImageMaskComponent` | Исходное изображение, цветовые слои и выделенные компоненты масок; статический или вращающийся режим |
| `TextObject` | Текст с атласом и резервом символов; наследует `StaticGroupObject` для предпросмотра |
| `ThreeDViewObject` | Область 3D-вида и аппаратные координаты/курс/высота/`yscreen`; XML-тип `layer3d` |

## Окна и панели

| Класс | Где искать | Назначение |
| --- | --- | --- |
| `MainWindow` | `src/windows` | Создание панелей и dock-окон, меню, команды, синхронизация интерфейса |
| `NewProjectDialog` | `src/windows` | Имя, размер, фон и путь нового проекта |
| `SettingsWindow` | `src/windows` | Глобальные настройки, ограничения памяти, справочник горячих клавиш |
| `FontExportDialog`, `FontExportDialogEntry` | `src/windows` | Выбор алфавитов экспортируемых шрифтов |
| `EditorWorkspacePanel` | `src/panels` | Компоновка холста, выбора рабочего слоя и панели действий |
| `ViewportPanel` | `src/panels` | Рисование сцены и редакторских элементов, обработка мыши/клавиатуры |
| `SpacingGuidePainter` | `src/panels` | Размерные стрелки и подписи интервалов с постоянным экранным размером |
| `ObjectLibraryPanel` | `src/panels` | Каталог добавляемых объектов и импорт изображений; локальный `DraggableToolButton` обеспечивает drag-and-drop |
| `ObjectListPanel` | `src/panels` | Объекты/группы, порядок, выделение, глазики, замочки и имена |
| `ObjectPropertiesPanel` | `src/panels` | Свойства одного или нескольких объектов; локальный `PropertyValueDelegate` выбирает редактор ячейки |
| `LayersPanel` | `src/panels` | Редакторские слои и операции над ними |
| `SelectionToolStrip` | `src/panels` | Кнопки существующих команд над выделением и истории |
| `ViewportSettingsPanel` | `src/panels` | Размер/фон холста и переключатели сетки/привязок |
| `FpgaStreamingPanel` | `src/panels` | Запуск BIN/UART/симулятора и экспорт конфигурации; локальные классы содержат поля и диалог настройки UART |
| `FpgaSimulatorPanel` | `src/panels` | Запуск программной модели и показ предупреждений |
| `FpgaScreenWidget` | `src/panels` | Отображение готового изображения симулятора |
| `AircraftPositionPanel` | `src/panels` | Поля и инструменты управления самолётом; внутренние `ScreenWidget`, `AltitudeSliderWidget`, `CourseKnobWidget` рисуют отдельные приборы |

Вспомогательные `AircraftValueSpin` и `AircraftScreenYSpin` в `AircraftPositionPanel.cpp` добавляют ускоренный шаг по Shift.

## Подготовка данных ПЛИС

| Класс/структура | Файл | Ответственность |
| --- | --- | --- |
| `FpgaXmlExporter` | `src/fpga/compiler/FpgaXmlExporter.*` | Аппаратный XML из объектов редактора: схемы, init/data, изображения и шрифты |
| `ImageResourceBuilder` | `src/fpga/compiler/ImageResourceBuilder.*` | Подготовка масок изображений под ограничения памяти и размеров |
| `FpgaCompiledDocument`, `FpgaCompiledObject` | `src/fpga/model/FpgaCompiledDocument.*` | Проверенное представление исходного XML до назначения аппаратных индексов |
| `FpgaSceneCompiler` | `src/fpga/compiler/FpgaSceneCompiler.*` | Аппаратные индексы, банки памяти, разворачивание текста в SYMBOLs |
| `FpgaScalePlaceholder` | `src/fpga/compiler/FpgaScalePlaceholder.*` | Фиксированный трёхсловный образ незавершённого модуля шкалы для XML и UART |
| `FpgaResourceLimits::Report` | `src/fpga/compiler/FpgaResourceLimits.*` | Ошибки и предупреждения по ресурсам объекта |
| `FpgaHardwareScene`, `FpgaHardwareObject`, `FpgaHardwareMemory`, `FpgaTextField` | `src/fpga/model/FpgaHardwareScene.h` | Готовые параметры, банки памяти и резерв надписей |
| `FpgaPipelineConfig`, `FpgaPipelineStage` | `src/fpga/config/FpgaPipelineConfig.*` | Структура аппаратного конвейера, сравнение и экспорт SystemVerilog |
| `FpgaProjectPacketBuilder` | `src/fpga/FpgaProjectPacketBuilder.*` | Единая сборка текущего проекта до bundle пакетов |
| `FpgaPacketCompiler` | `src/fpga/packets/FpgaPacketCompiler.*` | Упаковка параметров, статических масок, тайлов и команд 3D-вида |
| `FpgaPacketFramer` | `src/fpga/packets/FpgaPacketFramer.*` | Заголовок SAIN, длина и Fletcher32 |
| `FpgaPacket`, `FpgaPacketBundle` | `src/fpga/packets/FpgaPacket.*` | Один пакет и совокупность пакетов/сцены/предупреждений |
| `FpgaUartTransport`, `FpgaUartConfig` | `src/fpga/transport/FpgaUartTransport.*` | Рабочий поток последовательного порта и параметры сеанса |
| `FpgaPacketDump` | `src/fpga/transport/FpgaPacketDump.*` | Запись байтов bundle в BIN |
| `FpgaSimulator` | `src/fpga/simulator/FpgaSimulator.*` | Декодирование пакетов и расчёт изображения |
| `Layer3dFormat` | `src/fpga/model/Layer3dFormat.h` | Формат координат, курса и высоты, перевод единиц |

## Текст и небольшие утилиты

| Компонент | Папка/файл | Назначение |
| --- | --- | --- |
| `FpgaFont`, `FpgaGlyph` | `src/text/FontAtlas.h` | Атлас, порядок символов, маски, метрики и кернинг |
| `TextLayout`, `PositionedGlyph`, `TextMask` | `src/text/TextLayout.*` | Позиции символов и смешанная маска строки |
| `FontRasterizer` | `src/text/FontRasterizer.*` | Функции создания глифов/атласа средствами Qt |
| `FontAtlasXml` | `src/text/FontAtlasXml.*` | Проверка и чтение атласа из XML |
| `BitParser` | `src/utils/BitParser.h` | Чтение/запись битовых полей HEX и BGR-цвета |
| `NumericExpression::Parser` | `src/utils/NumericExpression.h` | Безопасный разбор числовых выражений |
| `ExpressionSpinBox`, `ExpressionDoubleSpinBox` | `src/utils/ExpressionSpinBox.h` | Поля целых/дробных чисел с арифметикой |
| `FlowLayout`, `FlowScrollArea` | `src/utils/FlowLayout.h` | Перенос элементов и согласование прокрутки с высотой содержимого |
| `ElidedLabel` | `src/utils/ElidedLabel.h` | Отображение длинной строки с многоточием |
| `ProportionalResizeResult` | `src/utils/ProportionalResize.h` | Общий расчёт пропорционального изменения размера |
| Остальные утилиты | `src/utils` | Настройки, русский перевод, жесты, границы альфа-маски, рисование обводки и подписи координат самолёта |
