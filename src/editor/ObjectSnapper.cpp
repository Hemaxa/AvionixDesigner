#include "ObjectSnapper.h"

#include <array>
#include <algorithm>
#include <limits>

namespace {
constexpr double epsilon = 1e-6;

std::array<double, 3> anchors(const QRectF &rect, Qt::Orientation axis)
{
    return axis == Qt::Horizontal
        ? std::array<double, 3>{rect.left(), rect.center().x(), rect.right()}
        : std::array<double, 3>{rect.top(), rect.center().y(), rect.bottom()};
}
}

ObjectSnapper::Result ObjectSnapper::snapMove(const QRectF &original, const QPointF &delta,
                                            const QList<QRectF> &targets, const Options &options)
{
    Result result;
    result.delta = {options.horizontalEnabled ? delta.x() : 0.0,
                    options.verticalEnabled ? delta.y() : 0.0};
    const QPointF rawDelta = result.delta;
    const QRectF raw = original.translated(result.delta);
    // Fallback — обычная привязка оси, если равный промежуток оказался недостижим.
    struct Fallback { double correction = 0.0; std::optional<Guide> guide; bool usedSpacing = false; };
    std::array<Fallback, 2> fallbacks;
    for (const auto axis : {Qt::Horizontal, Qt::Vertical}) {
        if (!(axis == Qt::Horizontal ? options.horizontalEnabled : options.verticalEnabled)) continue;
        const auto source = anchors(raw, axis);
        double correction = 0.0;
        double bestDistance = std::numeric_limits<double>::max();
        std::optional<Guide> guide;
        const auto guideAxis = axis == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal;
        if (options.gridEnabled) {
            const double step = qMax(1.0, options.gridStep);
            const double target = qRound(source[0] / step) * step;
            correction = target - source[0];
            guide = Guide{GuideKind::Grid, guideAxis, target};
        }
        auto consider = [&](double from, double to, GuideKind kind) {
            const double distance = qAbs(to - from);
            if (distance <= options.tolerance && distance < bestDistance - epsilon) {
                bestDistance = distance;
                correction = to - from;
                guide = Guide{kind, guideAxis, to};
            }
        };
        if (options.canvasEnabled) {
            const double extent = axis == Qt::Horizontal ? options.canvas.width() : options.canvas.height();
            // Край холста притягивает соответствующий край объекта, центр — центр.
            consider(source[0], 0.0, GuideKind::Canvas);
            consider(source[1], extent / 2.0, GuideKind::Canvas);
            consider(source[2], extent, GuideKind::Canvas);
        }
        if (options.objectsEnabled) {
            for (const auto &target : targets) {
                for (double from : source)
                    for (double to : anchors(target, axis))
                        consider(from, to, GuideKind::Object);
            }
        }
        auto &fallback = fallbacks[axis == Qt::Horizontal ? 0 : 1];
        fallback.correction = correction;
        fallback.guide = guide;
        if (options.objectsEnabled) {
            if (const auto spacing = EqualSpacing::find(raw, targets, axis, options.tolerance);
                spacing && qAbs(spacing->correction) < bestDistance - epsilon) {
                correction = spacing->correction;
                guide.reset();
                fallback.usedSpacing = true;
            }
        }
        if (axis == Qt::Horizontal) result.delta.rx() += correction;
        else result.delta.ry() += correction;
        if (guide) result.guides.append(*guide);
    }
    // Привязка по второй оси может вывести объект из строки/колонки.
    // В таком случае возвращаем обычную привязку, а не невидимое притяжение.
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto axis : {Qt::Horizontal, Qt::Vertical}) {
            auto &fallback = fallbacks[axis == Qt::Horizontal ? 0 : 1];
            if (!fallback.usedSpacing
                || EqualSpacing::find(original.translated(result.delta), targets, axis, 0.0)) continue;
            if (axis == Qt::Horizontal) result.delta.setX(rawDelta.x() + fallback.correction);
            else result.delta.setY(rawDelta.y() + fallback.correction);
            if (fallback.guide) result.guides.append(*fallback.guide);
            fallback.usedSpacing = false;
        }
    }
    validateGuides(result, original.translated(result.delta), targets, options);
    return result;
}

void ObjectSnapper::validateGuides(Result &result, const QRectF &actualBounds,
                                  const QList<QRectF> &targets, const Options &options)
{
    result.guides.removeIf([&](const Guide &guide) {
        const auto axis = guide.orientation == Qt::Vertical ? Qt::Horizontal : Qt::Vertical;
        const auto points = anchors(actualBounds, axis);
        if (guide.kind == GuideKind::Grid) return qAbs(points[0] - guide.position) > epsilon;
        return std::none_of(points.begin(), points.end(), [&](double point) {
            return qAbs(point - guide.position) <= epsilon;
        });
    });
    result.spacingGuides.clear();
    if (!options.objectsEnabled) return;
    for (const auto axis : {Qt::Horizontal, Qt::Vertical}) {
        if (!(axis == Qt::Horizontal ? options.horizontalEnabled : options.verticalEnabled)) continue;
        if (const auto match = EqualSpacing::find(actualBounds, targets, axis, 0.0))
            result.spacingGuides.append(match->guides);
    }
}
