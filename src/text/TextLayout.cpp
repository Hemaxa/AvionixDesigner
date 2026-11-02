// TextLayout - общий расчёт расположения букв для редактора и аппаратного экспорта.

#include "text/TextLayout.h"

#include <limits>

bool layoutText(const QString &text, const FpgaFont &font, TextLayout *layout, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    *layout = {};
    auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    qint64 penX = 0;
    qint64 minX = 0;
    qint64 maxX = 0;
    int top = 0;
    int bottom = 0;
    bool firstGlyph = true;
    QChar previous;
    QList<PositionedGlyph> positioned;
    const int fallbackSpace = qMax(4, font.size / 2);
    const int spaceAdvance = font.glyphs.contains(QLatin1Char(' '))
        ? font.glyphs.value(QLatin1Char(' ')).advance : fallbackSpace;

    for (const QChar character : text) {
        if (!previous.isNull())
            penX += font.kerningPairs.value(QString(previous) + character, 0);

        if (character.isSpace()) {
            // Как и в XML-редакторе, пробельные коды занимают место без статического объекта.
            penX += font.glyphs.contains(character) ? font.glyphs.value(character).advance : spaceAdvance;
        } else {
            const auto it = font.glyphs.constFind(character);
            if (it == font.glyphs.cend())
                return fail(QStringLiteral("В шрифте «%1» нет символа U+%2 (%3).")
                    .arg(font.name, QString::number(character.unicode(), 16).toUpper().rightJustified(4, '0'), QString(character)));
            const auto &glyph = it.value();
            if (glyph.width <= 0 || glyph.height <= 0 || glyph.advance < 0)
                return fail(QStringLiteral("Некорректные метрики символа U+%1.")
                    .arg(QString::number(character.unicode(), 16).toUpper()));

            const qint64 left = penX + glyph.bearingX;
            const qint64 right = left + glyph.width;
            minX = qMin(minX, left);
            maxX = qMax(maxX, right);
            if (left < std::numeric_limits<int>::min() || left > std::numeric_limits<int>::max())
                return fail(QStringLiteral("Координаты текста выходят за допустимый диапазон."));
            positioned.append({character, QPoint(static_cast<int>(left), glyph.bearingY)});
            if (firstGlyph) {
                top = glyph.bearingY;
                bottom = glyph.bearingY + glyph.height;
                firstGlyph = false;
            } else {
                top = qMin(top, glyph.bearingY);
                bottom = qMax(bottom, glyph.bearingY + glyph.height);
            }
            penX += glyph.advance;
        }
        minX = qMin(minX, penX);
        maxX = qMax(maxX, penX);
        previous = character;
    }

    if (maxX - minX > std::numeric_limits<int>::max()
        || minX < std::numeric_limits<int>::min() || minX > std::numeric_limits<int>::max())
        return fail(QStringLiteral("Ширина текста выходит за допустимый диапазон."));
    for (auto &glyph : positioned)
        glyph.position -= QPoint(static_cast<int>(minX), top);
    layout->glyphs = positioned;
    layout->size = QSize(qMax(1, static_cast<int>(maxX - minX)), qMax(1, bottom - top));
    return true;
}
