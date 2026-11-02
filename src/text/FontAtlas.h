// Метрики и растры шрифта в XML; эти структуры не являются аппаратными параметрами.
#pragma once

#include <QList>
#include <QMap>
#include <QString>

struct FpgaGlyph
{
    bool enabled = true;
    QChar literal;
    int code = 0;
    int width = 0;
    int height = 0;
    int advance = 0;
    int bearingX = 0;
    int bearingY = 0;
    int ascent = 0;
    int descent = 0;
    int offset = 0;
    int size = 14;
    QString maskRows;
};

struct FpgaFont
{
    int index = 0;
    QString name = QStringLiteral("Arial");
    int size = 14;
    int volume = 0;
    QList<QChar> glyphOrder;
    QMap<QChar, FpgaGlyph> glyphs;
    QMap<QString, int> kerningPairs;
    QString maskData;
};
