// RectanglePainting - отрисовка заливки и прямоугольной обводки по геометрии ПЛИС.

#pragma once

#include <QPainter>
#include <QPainterPath>

// a пикселей обводки снаружи заливки, без скошенных стыков и двойного смешивания.
inline void paintHardwareRectangle(QPainter &painter, const QRectF &inner,
                                    const QColor &fill, const QColor &stroke, double borderWidth)
{
    painter.save();
    painter.setPen(Qt::NoPen);
    if (borderWidth > 0.05 && stroke.alpha() > 0) {
        QPainterPath border;
        border.setFillRule(Qt::OddEvenFill);
        border.addRect(inner.adjusted(-borderWidth, -borderWidth, borderWidth, borderWidth));
        border.addRect(inner);
        painter.fillPath(border, stroke);
    }
    painter.fillRect(inner, fill);
    painter.restore();
}
