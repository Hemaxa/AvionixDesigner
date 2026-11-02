// FontRasterizer - создание растров и метрик символов средствами Qt.

#pragma once

#include "text/FontAtlas.h"

#include <QFont>

// Размер QFont задан в пикселях; результат содержит маску с прозрачной рамкой.
FpgaGlyph rasterizeGlyph(const QFont &font, QChar character);
// Оставляет один растр на уникальный код UTF-16 и рассчитывает пары кернинга.
FpgaFont rasterizeFont(const QFont &font, const QString &characters);
