//FpgaCompiledDocument - представление XML до расчёта аппаратной сцены.

#pragma once

#include "BaseObject.h"
#include "text/FontAtlas.h"

#include <QColor>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

class QDomDocument;

// Параметры исходного объекта XML; аппаратные индексы здесь ещё не назначаются.
struct FpgaCompiledObject
{
    QString type;
    bool leftAnchored = false;
    QStringList iparams;
    QString data;
};

// Исходный документ с ресурсами шрифтов; вход для FpgaSceneCompiler.
struct FpgaCompiledDocument
{
    QString projectName;
    int width = 0;
    int height = 0;
    QColor backgroundColor = Qt::black;
    QMap<QString, ParamSchema> schemas;
    QMap<int, FpgaFont> fonts;
    QList<FpgaCompiledObject> objects;
    QStringList warnings;

    bool isValid() const { return width > 0 && height > 0; }
    static FpgaCompiledDocument fromDomDocument(const QDomDocument &dom, QString *errorMessage = nullptr);
};
