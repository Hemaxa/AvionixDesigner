// RasterGeometry - вычисление границ непрозрачной части растровой маски.

#pragma once

#include <QImage>
#include <QRect>

inline QRect rasterAlphaBounds(const QImage &source)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    int left=image.width(), top=image.height(), right=-1, bottom=-1;
    for (int y=0; y<image.height(); ++y) {
        const auto *row=reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x=0; x<image.width(); ++x) {
            if (!qAlpha(row[x])) continue;
            left=qMin(left,x); right=qMax(right,x); top=qMin(top,y); bottom=qMax(bottom,y);
        }
    }
    return right < left ? QRect() : QRect(QPoint(left,top),QPoint(right,bottom));
}
