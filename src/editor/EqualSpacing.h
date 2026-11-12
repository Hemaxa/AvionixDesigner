// Геометрия равных промежутков. Не зависит от сцены, виджетов и формата ПЛИС.
#pragma once

#include <QList>
#include <QPointF>
#include <QRectF>
#include <optional>

// SpacingGuide — пара границ соседей для размерной стрелки и подписи интервала.
struct SpacingGuide
{
    Qt::Orientation orientation = Qt::Horizontal;
    QRectF before;
    QRectF after;

    double distance() const;
};

// EqualSpacing — поиск равных промежутков в общей строке или колонке объектов.
class EqualSpacing
{
public:
    // Match — поправка одной координаты и два подтверждающих равенство промежутка.
    struct Match
    {
        double correction = 0.0;
        QList<SpacingGuide> guides;
    };

    // Рассматриваются соседние объекты в общей строке/колонке:
    // вставка между двумя объектами и продолжение ряда с любого конца.
    static std::optional<Match> find(const QRectF &moving, const QList<QRectF> &targets,
                                     Qt::Orientation orientation, double tolerance);
};
