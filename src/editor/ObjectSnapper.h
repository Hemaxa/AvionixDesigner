// Выбор ближайшей привязки при перемещении без зависимости от QWidget.
#pragma once

#include "EqualSpacing.h"

// ObjectSnapper — выбор ближайшей привязки перемещения и проверка её подсказок.
class ObjectSnapper
{
public:
    enum class GuideKind { Canvas, Grid, Object };
    // Guide — линия совпадения края или центра с холстом, сеткой или объектом.
    struct Guide
    {
        GuideKind kind = GuideKind::Object;
        Qt::Orientation orientation = Qt::Vertical;
        double position = 0.0;
    };
    // Options — доступные источники привязок, оси и порог в координатах холста.
    struct Options
    {
        QSizeF canvas;
        double tolerance = 8.0; // В координатах холста; вызывающий учитывает масштаб.
        double gridStep = 1.0;
        bool canvasEnabled = false;
        bool gridEnabled = false;
        bool objectsEnabled = false;
        bool horizontalEnabled = true;
        bool verticalEnabled = true;
    };
    // Result — итоговое смещение и геометрия редакторских подсказок.
    struct Result
    {
        QPointF delta;
        QList<Guide> guides;
        QList<SpacingGuide> spacingGuides;
    };

    static Result snapMove(const QRectF &original, const QPointF &delta,
                           const QList<QRectF> &targets, const Options &options);
    // Повторная проверка после ограничений холста/симметрии: подсказки
    // описывают только фактически достижимое положение.
    static void validateGuides(Result &result, const QRectF &actualBounds,
                               const QList<QRectF> &targets, const Options &options);
};
