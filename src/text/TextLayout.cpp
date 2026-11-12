// TextLayout - общий расчёт расположения букв для редактора и аппаратного экспорта.

#include "text/TextLayout.h"

#include <limits>
#include <QRect>
#include <QRegularExpression>
#include <QVector>
#include <QtMath>

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

    for (qsizetype index = 0; index < text.size(); ++index) {
        const QChar character = text[index];
        if (!previous.isNull())
            penX += font.kerningPairs.value(QString(previous) + character, 0);

        if (character.isSpace()) {
            // Пробел сдвигает перо, оставляя соответствующую позицию без маски.
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
            positioned.append({character, QPoint(static_cast<int>(left), glyph.bearingY), index});
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

bool composeTextMask(const QString &text, const FpgaFont &font, TextMask *mask,
                     QString *errorMessage, qint64 maxPixels)
{
    *mask = {};
    TextLayout layout;
    if (!layoutText(text, font, &layout, errorMessage))
        return false;
    mask->layoutSize = layout.size;
    QRect bounds;
    // Сначала находим полезные пиксели, не выделяя память под прозрачные поля.
    for (const auto &positioned : layout.glyphs) {
        const auto &glyph = font.glyphs[positioned.character];
        if (!glyph.enabled) continue;
        QString pixels = glyph.maskRows;
        pixels.remove('\n'); pixels.remove('\r');
        if (pixels.size() != qint64(glyph.width) * glyph.height || pixels.contains(QRegularExpression("[^0-7]"))) {
            if (errorMessage) *errorMessage = QStringLiteral("Повреждена маска символа U+%1.").arg(QString::number(glyph.code, 16));
            return false;
        }
        for (int y = 0; y < glyph.height; ++y)
            for (int x = 0; x < glyph.width; ++x)
                if (pixels[y * glyph.width + x] != QLatin1Char('0'))
                    bounds = bounds.united(QRect(positioned.position + QPoint(x, y), QSize(1, 1)));
    }
    if (bounds.isEmpty()) return true;
    const qint64 area = qint64(bounds.width()) * bounds.height();
    if (area > maxPixels || area > std::numeric_limits<int>::max()) {
        if (errorMessage) *errorMessage = QStringLiteral("Маске текста требуется %1 пикселей; доступно %2. Уменьшите размер текста.").arg(area).arg(maxPixels);
        return false;
    }
    QVector<double> alpha(area, 0.0);
    for (const auto &positioned : layout.glyphs) {
        const auto &glyph = font.glyphs[positioned.character];
        if (!glyph.enabled) continue;
        QString pixels = glyph.maskRows;
        pixels.remove('\n'); pixels.remove('\r');
        for (int y = 0; y < glyph.height; ++y) {
            for (int x = 0; x < glyph.width; ++x) {
                const int level = pixels[y * glyph.width + x].digitValue();
                if (!level) continue;
                const QPoint p = positioned.position + QPoint(x, y) - bounds.topLeft();
                double &a = alpha[p.y() * bounds.width() + p.x()];
                const double source = (level + 1) / 8.0;
                a = source + a * (1.0 - source);
            }
        }
    }
    mask->offset = bounds.topLeft();
    mask->size = bounds.size();
    mask->data.reserve(area);
    for (double a : alpha)
        mask->data.append(QChar('0' + (a == 0.0 ? 0 : qBound(1, qRound(a * 8.0) - 1, 7))));
    return true;
}
