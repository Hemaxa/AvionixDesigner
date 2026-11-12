#pragma once

#include "EqualSpacing.h"
#include <QColor>

class QPainter;

// SpacingGuidePainter — размерные стрелки и подписи равных промежутков.
// Размер элементов постоянен на экране при любом масштабе холста.
class SpacingGuidePainter
{
public:
    static void draw(QPainter &painter, const QList<SpacingGuide> &guides,
                     const QColor &color, const QColor &background);
};
