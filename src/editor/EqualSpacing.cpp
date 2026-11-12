#include "EqualSpacing.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr double epsilon = 1e-6;

double start(const QRectF &rect, Qt::Orientation axis)
{
    return axis == Qt::Horizontal ? rect.left() : rect.top();
}

double end(const QRectF &rect, Qt::Orientation axis)
{
    return axis == Qt::Horizontal ? rect.right() : rect.bottom();
}

Qt::Orientation perpendicular(Qt::Orientation axis)
{
    return axis == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal;
}

bool valid(const QRectF &rect)
{
    return !rect.isEmpty() && std::isfinite(rect.x()) && std::isfinite(rect.y())
        && std::isfinite(rect.right()) && std::isfinite(rect.bottom());
}
}

double SpacingGuide::distance() const
{
    return start(after, orientation) - end(before, orientation);
}

std::optional<EqualSpacing::Match> EqualSpacing::find(
    const QRectF &moving, const QList<QRectF> &targets, Qt::Orientation axis, double tolerance)
{
    if (!valid(moving) || !std::isfinite(tolerance) || tolerance < 0.0)
        return std::nullopt;

    const auto cross = perpendicular(axis);
    QList<QRectF> neighbors;
    for (const auto &rect : targets) {
        if (valid(rect) && qMin(end(rect, cross), end(moving, cross))
                - qMax(start(rect, cross), start(moving, cross)) > epsilon)
            neighbors.append(rect);
    }
    std::sort(neighbors.begin(), neighbors.end(), [axis, cross](const QRectF &a, const QRectF &b) {
        if (start(a, axis) != start(b, axis)) return start(a, axis) < start(b, axis);
        if (end(a, axis) != end(b, axis)) return end(a, axis) < end(b, axis);
        return start(a, cross) < start(b, cross);
    });

    std::optional<Match> best;
    double bestSpan = std::numeric_limits<double>::max();
    const double size = end(moving, axis) - start(moving, axis);
    for (qsizetype i = 0; i + 1 < neighbors.size(); ++i) {
        const auto &a = neighbors[i];
        const auto &b = neighbors[i + 1];
        // Все три объекта должны пересекаться одной поперечной прямой.
        if (std::min({end(a, cross), end(b, cross), end(moving, cross)})
                - std::max({start(a, cross), start(b, cross), start(moving, cross)}) <= epsilon)
            continue;
        const double gap = start(b, axis) - end(a, axis);
        if (gap < -epsilon) continue;

        auto consider = [&](double position, int placement) {
            const double correction = position - start(moving, axis);
            if (qAbs(correction) > tolerance + epsilon) return;
            const QRectF moved = moving.translated(axis == Qt::Horizontal
                ? QPointF(correction, 0) : QPointF(0, correction));
            // Продолжение ряда не должно перескакивать через соседний объект.
            if (placement < 0 && i > 0 && start(moved, axis) < end(neighbors[i - 1], axis) - epsilon) return;
            if (placement > 0 && i + 2 < neighbors.size()
                && end(moved, axis) > start(neighbors[i + 2], axis) + epsilon) return;
            const double span = qMax(end(moved, axis), end(b, axis))
                - qMin(start(moved, axis), start(a, axis));
            if (best && (qAbs(correction) > qAbs(best->correction) + epsilon
                || (qAbs(qAbs(correction) - qAbs(best->correction)) <= epsilon && span >= bestSpan))) return;

            Match match;
            match.correction = correction;
            if (placement < 0) match.guides = {{axis, moved, a}, {axis, a, b}};
            else if (placement > 0) match.guides = {{axis, a, b}, {axis, b, moved}};
            else match.guides = {{axis, a, moved}, {axis, moved, b}};
            best = match;
            bestSpan = span;
        };

        consider(start(a, axis) - gap - size, -1);
        if (gap >= size - epsilon)
            consider(end(a, axis) + (gap - size) / 2.0, 0);
        consider(end(b, axis) + gap, 1);
    }
    return best;
}
