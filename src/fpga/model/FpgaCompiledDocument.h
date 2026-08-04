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

struct FpgaCompiledObject
{
    QString type;
    QStringList iparams;
    QString data;
    int startParamIndex = 0;
    int paramCount = 0;
    int memId = -1;
    FpgaMemoryKind memoryKind = FpgaMemoryKind::None;
};

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

