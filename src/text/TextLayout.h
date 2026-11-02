// Единственный расчёт координат глифов для редактора и подготовки аппаратной сцены.
#pragma once

#include "text/FontAtlas.h"

#include <QPoint>
#include <QSize>

struct PositionedGlyph
{
    QChar character;
    QPoint position;
};

struct TextLayout
{
    QList<PositionedGlyph> glyphs;
    QSize size = QSize(1, 1);
};

bool layoutText(const QString &text, const FpgaFont &font, TextLayout *layout,
                QString *errorMessage = nullptr);
