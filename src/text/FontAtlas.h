// FontAtlas - метрики, растры и кернинг шрифтовых ресурсов XML.

#pragma once

#include <QList>
#include <QMap>
#include <QString>

// FpgaGlyph - метрики и 3-битная маска одного символа шрифтового атласа.
struct FpgaGlyph
{
    bool enabled = true;
    QChar literal;
    int code = 0;
    int width = 0;
    int height = 0;
    int advance = 0;  // Смещение пера после символа, в пикселях; ноль допустим.
    int bearingX = 0; // Положение начала маски относительно пера, включая рамку.
    int bearingY = 0; // Положение начала маски относительно базовой линии.
    int ascent = 0;
    int descent = 0;
    int offset = 0; // Индекс первого элемента в общем растре, не битовое смещение.
    int size = 14;
    QString maskRows; // Уровни 0…7; строки разделены переводом строки.
};

// FpgaFont - алфавит одного шрифта и размера с общими растрами и парами кернинга.
struct FpgaFont
{
    int index = 0;
    QString name = QStringLiteral("Arial");
    int size = 14;
    int volume = 0; // Объём XML-ресурса в битах: число элементов маски × 3.
    QList<QChar> glyphOrder;
    QMap<QChar, FpgaGlyph> glyphs;
    QMap<QString, int> kerningPairs;
    QString maskData; // Все маски подряд: один символ '0'…'7' на ячейку.

    // Сохраняет порядок XML/атласа; недостающие в glyphOrder коды дополняет из glyphs.
    QString orderedCharacters() const
    {
        QString characters;
        for (const QChar ch : glyphOrder)
            if (glyphs.contains(ch) && !characters.contains(ch)) characters.append(ch);
        for (auto it = glyphs.cbegin(); it != glyphs.cend(); ++it)
            if (!characters.contains(it.key())) characters.append(it.key());
        return characters;
    }
};
