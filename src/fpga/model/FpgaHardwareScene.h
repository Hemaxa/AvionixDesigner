// Аппаратная сцена: только готовые слова параметров и адресуемые банки растров.
#pragma once

#include "BaseObject.h"

#include <QColor>
#include <QList>
#include <QMap>
#include <QStringList>

enum class FpgaMemoryKind { StaticMask, RotationTiles };

struct FpgaHardwareMemory
{
    FpgaMemoryKind kind = FpgaMemoryKind::StaticMask;
    QString data;
};

struct FpgaHardwareObject
{
    QString type;
    bool leftAnchored = false;
    QStringList iparams;
    int startParamIndex = 0;
    int memId = -1;
};

struct FpgaHardwareScene
{
    QString projectName;
    int width = 0;
    int height = 0;
    QColor backgroundColor = Qt::black;
    QMap<QString, ParamSchema> schemas;
    QList<FpgaHardwareObject> objects;
    QList<FpgaHardwareMemory> memories;
    QStringList warnings;

    bool isValid() const { return width > 0 && height > 0; }
};
