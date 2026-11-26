// ScaleGlyphs — встроенные цифры 5×7 для приближённого рисунка процедурных шкал.
#pragma once

#include <QPainter>

namespace ScaleGlyphs {
inline int textWidth(const QString &text, int cell)
{
    return qMax(0, int(text.size()) * 6 - 1) * cell;
}

inline void drawNumber(QPainter &painter, const QString &text, const QPointF &topLeft,
                       int cell, const QColor &color)
{
    static constexpr quint8 rows[10][7] = {
        {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
        {14,17,17,15,1,1,14}
    };
    for (int i = 0; i < text.size(); ++i) {
        const int digit = text[i].digitValue();
        if (digit < 0) continue;
        for (int y = 0; y < 7; ++y) for (int x = 0; x < 5; ++x) {
            if (rows[digit][y] & (1 << (4 - x)))
                painter.fillRect(QRectF(topLeft + QPointF((i * 6 + x) * cell, y * cell), QSizeF(cell, cell)), color);
        }
    }
}
}
