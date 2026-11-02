#pragma once

#include "text/FontAtlas.h"

#include <QFont>

FpgaGlyph rasterizeGlyph(const QFont &font, QChar character);
FpgaFont rasterizeFont(const QFont &font, const QString &characters);
