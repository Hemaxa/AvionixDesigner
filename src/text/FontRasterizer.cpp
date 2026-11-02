#include "text/FontRasterizer.h"

#include <QFontMetrics>
#include <QPainter>

FpgaGlyph rasterizeGlyph(const QFont &font, QChar character)
{
    const QFontMetrics metrics(font);
    QRect tight = metrics.tightBoundingRect(QString(character));
    if (tight.isEmpty())
        tight = QRect(0, -metrics.ascent(), qMax(1, metrics.horizontalAdvance(character)), metrics.height());

    const int padding = qMax(2, (qMax(1, font.pixelSize()) + 7) / 8);
    QImage image(tight.width() + 2 * padding, tight.height() + 2 * padding, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QPoint(padding - tight.left(), padding - tight.top()), QString(character));
    painter.end();

    FpgaGlyph glyph;
    glyph.literal = character;
    glyph.code = character.unicode();
    glyph.width = image.width();
    glyph.height = image.height();
    glyph.advance = metrics.horizontalAdvance(character);
    // Bearing указывает начало сохранённой маски, включая прозрачную рамку.
    glyph.bearingX = tight.left() - padding;
    glyph.bearingY = tight.top() - padding;
    glyph.ascent = metrics.ascent();
    glyph.descent = metrics.descent();
    glyph.size = font.pixelSize();
    QStringList rows;
    for (int y = 0; y < image.height(); ++y) {
        QString row;
        for (int x = 0; x < image.width(); ++x)
            row.append(QChar('0' + qBound(0, qRound(image.pixelColor(x, y).alpha() * 7.0 / 255.0), 7)));
        rows.append(row);
    }
    glyph.maskRows = rows.join('\n');
    return glyph;
}

FpgaFont rasterizeFont(const QFont &font, const QString &characters)
{
    FpgaFont atlas;
    atlas.name = font.family();
    atlas.size = font.pixelSize();
    for (const QChar character : characters) {
        if (!atlas.glyphs.contains(character)) {
            atlas.glyphOrder.append(character);
            atlas.glyphs.insert(character, rasterizeGlyph(font, character));
        }
    }
    const QFontMetrics metrics(font);
    for (const QChar left : atlas.glyphOrder) {
        for (const QChar right : atlas.glyphOrder) {
            const QString pair = QString(left) + right;
            const int delta = metrics.horizontalAdvance(pair)
                - atlas.glyphs.value(left).advance - atlas.glyphs.value(right).advance;
            if (delta != 0)
                atlas.kerningPairs.insert(pair, delta);
        }
    }
    return atlas;
}
