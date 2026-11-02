// TextLayout - общий расчёт расположения букв для редактора и аппаратного экспорта.

#pragma once

#include "text/FontAtlas.h"

#include <QPoint>
#include <QSize>

// PositionedGlyph - символ и рассчитанная позиция его маски внутри текстового блока.
struct PositionedGlyph
{
    QChar character;
    QPoint position;
};

// TextLayout - общий расчёт расположения букв для редактора и аппаратного экспорта.
struct TextLayout
{
    QList<PositionedGlyph> glyphs;
    QSize size = QSize(1, 1);
};

// Заполняет ненулевой layout координатами от верхнего левого угла текстового блока.
// Пробелы изменяют положение пера без создания маски; неизвестный глиф даёт ошибку.
bool layoutText(const QString &text, const FpgaFont &font, TextLayout *layout,
                QString *errorMessage = nullptr);
