//ProportionalResize - утилиты для пропорционального изменения размеров объектов

#pragma once

#include <QRectF>
#include <QtMath>

struct ProportionalResizeResult
{
    QRectF rect;
    double scale = 1.0;
};

inline ProportionalResizeResult proportionalResizeRect(const QRectF &sourceRect, int edgeFlags, double dx, double dy)
{
    if (sourceRect.isEmpty())
        return {sourceRect, 1.0};

    QRectF tentative = sourceRect;
    if (edgeFlags & 1)
        tentative.setLeft(tentative.left() + dx);
    if (edgeFlags & 2)
        tentative.setRight(tentative.right() + dx);
    if (edgeFlags & 4)
        tentative.setTop(tentative.top() + dy);
    if (edgeFlags & 8)
        tentative.setBottom(tentative.bottom() + dy);

    const double oldWidth = qMax(1.0, sourceRect.width());
    const double oldHeight = qMax(1.0, sourceRect.height());
    const double widthScale = qMax(1.0 / oldWidth, tentative.width() / oldWidth);
    const double heightScale = qMax(1.0 / oldHeight, tentative.height() / oldHeight);

    const bool hasHorizontal = (edgeFlags & 1) || (edgeFlags & 2);
    const bool hasVertical = (edgeFlags & 4) || (edgeFlags & 8);

    double uniformScale = 1.0;
    if (hasHorizontal && !hasVertical) {
        uniformScale = widthScale;
    } else if (!hasHorizontal && hasVertical) {
        uniformScale = heightScale;
    } else {
        uniformScale = qAbs(widthScale - 1.0) >= qAbs(heightScale - 1.0) ? widthScale : heightScale;
    }

    const double newWidth = qMax(1.0, oldWidth * uniformScale);
    const double newHeight = qMax(1.0, oldHeight * uniformScale);

    QRectF result = sourceRect;
    if ((edgeFlags & 1) && !(edgeFlags & 2)) {
        result.setLeft(sourceRect.right() - newWidth);
        result.setRight(sourceRect.right());
    } else if ((edgeFlags & 2) && !(edgeFlags & 1)) {
        result.setLeft(sourceRect.left());
        result.setRight(sourceRect.left() + newWidth);
    } else {
        result.setLeft(sourceRect.center().x() - newWidth / 2.0);
        result.setRight(sourceRect.center().x() + newWidth / 2.0);
    }

    if ((edgeFlags & 4) && !(edgeFlags & 8)) {
        result.setTop(sourceRect.bottom() - newHeight);
        result.setBottom(sourceRect.bottom());
    } else if ((edgeFlags & 8) && !(edgeFlags & 4)) {
        result.setTop(sourceRect.top());
        result.setBottom(sourceRect.top() + newHeight);
    } else {
        result.setTop(sourceRect.center().y() - newHeight / 2.0);
        result.setBottom(sourceRect.center().y() + newHeight / 2.0);
    }

    return {result.normalized(), uniformScale};
}
