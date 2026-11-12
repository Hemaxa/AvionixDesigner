// FpgaHardwareScene - готовые аппаратные параметры объектов и общие банки растров.

#pragma once

#include "BaseObject.h"
#include "text/FontAtlas.h"

#include <QColor>
#include <QList>
#include <QMap>
#include <QStringList>

// FpgaMemoryKind - способ упаковки растрового банка в пакеты ПЛИС.
enum class FpgaMemoryKind { StaticMask, RotationTiles, FontAtlas };

// FpgaTextField - резерв слов параметров и шрифт надписи в текущем сеансе UART.
struct FpgaTextField
{
    int fontIndex = 0;
    int capacity = 0;
};

// FpgaHardwareMemory - общий банк уровней маски до упаковки в аппаратные блоки.
struct FpgaHardwareMemory
{
    FpgaMemoryKind kind = FpgaMemoryKind::StaticMask;
    QString data; // Плотная строка уровней 0…7; тайлы упаковываются позже.
};

// FpgaHardwareObject - аппаратные слова объекта и ссылки на параметры и память сцены.
struct FpgaHardwareObject
{
    QString type;
    bool leftAnchored = false;
    QStringList iparams; // Слова по 150 бит: 38 HEX-символов на init.
    int startParamIndex = 0; // Глобальный индекс первого слова параметров.
    int memId = -1; // Индекс общего банка в memories; -1 означает отсутствие маски.
};

// FpgaHardwareScene - готовые аппаратные параметры объектов и общие банки растров.
struct FpgaHardwareScene
{
    QString projectName;
    int width = 0;
    int height = 0;
    QColor backgroundColor = Qt::black;
    QMap<QString, ParamSchema> schemas;
    QList<FpgaHardwareObject> objects;
    QList<FpgaHardwareMemory> memories;
    QMap<int, FpgaFont> fonts;
    QList<FpgaTextField> textFields;
    QStringList warnings;

    bool isValid() const { return width > 0 && height > 0; }
};
