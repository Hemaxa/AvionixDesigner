//FpgaCompiledDocument - типизированное представление строгого XML, готового к загрузке в ПЛИС

#pragma once

#include "BaseObject.h"

#include <QColor>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

class QDomDocument;

enum class FpgaMemoryKind
{
    None,
    StaticMask,
    RotationTiles
};

// Объект после экспорта: слова init, маска и назначенные индексы параметров/памяти.
struct FpgaCompiledObject
{
    QString type;
    bool leftAnchored = false;
    QStringList iparams;
    QString data;
    QMap<QString, int> kerningPairs;
    int startParamIndex = 0;
    int paramCount = 0;
    int memId = -1;
    FpgaMemoryKind memoryKind = FpgaMemoryKind::None;
};

// Подготовленный кадр со схемами; общий вход для компилятора пакетов и симулятора.
struct FpgaCompiledDocument
{
    QString projectName;
    int width = 0;
    int height = 0;
    QColor backgroundColor = Qt::black;
    QMap<QString, ParamSchema> schemas;
    QList<FpgaCompiledObject> objects;
    QStringList warnings;

    bool isValid() const { return width > 0 && height > 0; }
    static FpgaCompiledDocument fromDomDocument(const QDomDocument &dom, QString *errorMessage = nullptr);
};
