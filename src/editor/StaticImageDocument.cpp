// StaticImageDocument — разбиение без интерполяции и операции над уровнями масок 0…7.
#include "StaticImageDocument.h"
#include "ImageObject.h"
#include "OptimizedStaticImageObject.h"
#include "StaticGroupObject.h"
#include "TextObject.h"
#include "fpga/compiler/FpgaResourceLimits.h"
#include <QBitArray>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace
{
constexpr qint64 maxCells = 64 * 1024 * 1024;
QRect groupBounds(const StaticGroupObject &group)
{
    QRect bounds;
    for (const auto &state : group.states)
        bounds = bounds.united(QRect(state.x, state.y, state.w, state.h));
    return bounds;
}
} // namespace

StaticImageMask StaticImageMask::fromImage(const QRect &rect, const QColor &color, const QImage &image,
                                           bool enabled)
{
    StaticImageMask result{rect, color, {}, enabled};
    if (rect.isEmpty() || qint64(rect.width()) * rect.height() > maxCells)
        return result;
    result.data.resize(rect.width() * rect.height());
    for (int y = 0; y < rect.height(); ++y)
        for (int x = 0; x < rect.width(); ++x)
            result.data[y * rect.width() + x] =
                image.isNull()
                    ? 7
                    : (image.rect().contains(x, y) ? qRound(image.pixelColor(x, y).alpha() * 7.0 / 255.0)
                                                   : 0);
    return result;
}

StaticImageMask StaticImageMask::clipped(const QRect &bounds) const
{
    const QRect intersection = rect.intersected(bounds);
    StaticImageMask result{intersection, color, {}, enabled};
    result.data.reserve(intersection.width() * intersection.height());
    for (int y = intersection.top(); y <= intersection.bottom(); ++y)
        result.data.append(
            data.mid((y - rect.y()) * rect.width() + intersection.x() - rect.x(), intersection.width()));
    return result;
}

QImage StaticImageMask::image() const
{
    if (data.size() != qint64(rect.width()) * rect.height())
        return {};
    QImage result(rect.size(), QImage::Format_ARGB32);
    for (int y = 0; y < rect.height(); ++y)
        for (int x = 0; x < rect.width(); ++x)
        {
            QColor pixel = color;
            pixel.setAlpha(qRound(static_cast<quint8>(data[y * rect.width() + x]) * 255.0 / 7.0));
            result.setPixelColor(x, y, pixel);
        }
    return result;
}

StaticMaskAnalysis StaticImageMask::analyze() const
{
    StaticMaskAnalysis result;
    if (rect.isEmpty() || data.size() != qint64(rect.width()) * rect.height())
        return result;
    const int width = rect.width(), height = rect.height();
    QBitArray visited(data.size());
    auto available = [&](int x, int y)
    {
        const int i = y * width + x;
        return data[i] != 0 && !visited.testBit(i);
    };
    // Fill horizontal spans: memory use stays small for large solid masks.
    QList<QPoint> pending;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            if (!available(x, y))
                continue;
            QRect bounds;
            qint64 pixels = 0;
            pending.append(QPoint(x, y));
            while (!pending.isEmpty())
            {
                const auto seed = pending.takeLast();
                if (!available(seed.x(), seed.y()))
                    continue;
                int left = seed.x(), right = seed.x();
                while (left > 0 && available(left - 1, seed.y()))
                    --left;
                while (right + 1 < width && available(right + 1, seed.y()))
                    ++right;
                for (int column = left; column <= right; ++column)
                    visited.setBit(seed.y() * width + column);
                pixels += right - left + 1;
                bounds = bounds.united(QRect(left, seed.y(), right - left + 1, 1));
                for (int row : {seed.y() - 1, seed.y() + 1})
                {
                    if (row < 0 || row >= height)
                        continue;
                    bool inRun = false;
                    for (int column = qMax(0, left - 1); column <= qMin(width - 1, right + 1); ++column)
                    {
                        const bool filled = available(column, row);
                        if (filled && !inRun)
                            pending.append(QPoint(column, row));
                        inRun = filled;
                    }
                }
            }
            bounds.translate(rect.topLeft());
            result.bounds = result.bounds.united(bounds);
            result.pixels += pixels;
            ++result.components;
            if (pixels <= 4 && result.fragments.size() < 256)
                result.fragments.append(bounds);
        }
    if (result.components < 2)
        result.fragments.clear();
    return result;
}

bool StaticImageDocument::eraseMasks(int region, const QList<int> &indexes, QPointF from, QPointF to,
                                     int diameter)
{
    if (region < 0 || region >= regions.size() || diameter < 1 || diameter > 64)
        return false;
    const auto area = QRectF(regions[region].rect).adjusted(-diameter, -diameter, diameter, diameter);
    const QPointF delta = to - from;
    double begin = 0, end = 1;
    auto clip = [&](double p, double q)
    {
        if (qFuzzyIsNull(p))
            return q >= 0;
        const double t = q / p;
        if (p < 0)
            begin = qMax(begin, t);
        else
            end = qMin(end, t);
        return begin <= end;
    };
    if (!clip(-delta.x(), from.x() - area.left()) || !clip(delta.x(), area.right() - from.x()) ||
        !clip(-delta.y(), from.y() - area.top()) || !clip(delta.y(), area.bottom() - from.y()))
        return false;
    to = from + delta * end;
    from += delta * begin;
    const int steps = qMax(1, int(std::ceil(qMax(std::abs(to.x() - from.x()), std::abs(to.y() - from.y())))));
    bool changed = false;
    for (int index : QSet<int>(indexes.begin(), indexes.end()))
    {
        if (index < 0 || index >= regions[region].masks.size())
            continue;
        auto &mask = regions[region].masks[index];
        for (int step = 0; step <= steps; ++step)
        {
            const auto point = from + (to - from) * (double(step) / steps);
            const QRect brush(int(std::floor(point.x())) - diameter / 2,
                              int(std::floor(point.y())) - diameter / 2, diameter, diameter);
            const auto hit = brush.intersected(mask.rect);
            for (int y = hit.top(); y <= hit.bottom(); ++y)
                for (int x = hit.left(); x <= hit.right(); ++x)
                {
                    auto &value = mask.data[(y - mask.rect.y()) * mask.rect.width() + x - mask.rect.x()];
                    changed |= value != 0;
                    value = 0;
                }
        }
    }
    return changed;
}

qint64 StaticImageRegion::cells() const
{
    qint64 count = 0;
    for (const auto &mask : masks)
        count += mask.data.size();
    return count;
}

bool StaticImageRegion::empty() const
{
    for (const auto &mask : masks)
        for (const auto value : mask.data)
            if (value != 0)
                return false;
    return true;
}

bool StaticImageDocument::supports(const BaseObject *object)
{
    if (!object || dynamic_cast<const TextObject *>(object))
        return false;
    if (const auto *image = dynamic_cast<const ImageObject *>(object))
        return !image->isRotationObject();
    return dynamic_cast<const StaticGroupObject *>(object) ||
           dynamic_cast<const OptimizedStaticImageObject *>(object);
}

QPointF StaticImageDocument::objectOrigin(const BaseObject &object)
{
    if (const auto *group = dynamic_cast<const StaticGroupObject *>(&object))
        return groupBounds(*group).topLeft();
    return object.getBoundingRect().topLeft();
}

StaticImageDocument StaticImageDocument::fromObject(const BaseObject &object, QString *error)
{
    if (error)
        error->clear();
    if (const auto *optimized = dynamic_cast<const OptimizedStaticImageObject *>(&object))
        return optimized->document;
    StaticImageDocument result;
    qint64 originalEnd = -1;
    StaticImageRegion region;
    region.name = QStringLiteral("Часть 1");
    if (const auto *image = dynamic_cast<const ImageObject *>(&object); image && !image->isRotationObject())
    {
        result.size = QSize(qMax(1, qRound(image->width)), qMax(1, qRound(image->height)));
        if (qint64(result.size.width()) * result.size.height() > maxCells)
        {
            if (error)
                *error = QStringLiteral(
                    "Изображение превышает 64 млн пикселей. Уменьшите его перед редактированием.");
            return {};
        }
        for (const auto &component : image->maskComponents())
            region.masks.append(
                StaticImageMask::fromImage(component.bounds, component.color, component.mask));
        if (region.masks.isEmpty())
            region.masks.append(StaticImageMask::fromImage(
                QRect(QPoint(), result.size), image->effectiveMaskColor(), image->renderedImage()));
    }
    else if (const auto *group = dynamic_cast<const StaticGroupObject *>(&object);
             group && !dynamic_cast<const TextObject *>(group))
    {
        const auto bounds = groupBounds(*group);
        result.size = bounds.size();
        for (int i = 0; i < group->states.size(); ++i)
        {
            const auto &state = group->states[i];
            originalEnd = qMax(originalEnd, qint64(state.addr) + qint64(state.w) * state.h);
            const QRect rect(QPoint(state.x, state.y) - bounds.topLeft(), QSize(state.w, state.h));
            region.masks.append(
                StaticImageMask::fromImage(rect, state.color, group->maskImages.value(i), state.enabled));
        }
    }
    region.rect = QRect(QPoint(), result.size);
    result.originalCells = originalEnd < 0 ? region.cells() : originalEnd;
    result.regions = {region};
    if (!result.validate(error))
        return {};
    return result;
}

bool StaticImageDocument::validate(QString *error) const
{
    auto fail = [&](const QString &message)
    {
        if (error)
            *error = message;
        return false;
    };
    const QRect canvas(QPoint(), size);
    if (size.isEmpty() || qint64(size.width()) * size.height() > maxCells || regions.isEmpty() ||
        regions.size() > 4096 || originalCells < 0 || originalCells > maxCells || nextId <= 0)
        return fail(QStringLiteral("Некорректный размер или состав статического изображения."));
    QSet<int> ids;
    qint64 area = 0, dataSize = 0;
    for (int i = 0; i < regions.size(); ++i)
    {
        const auto &region = regions[i];
        if (region.id <= 0 || region.id >= nextId || ids.contains(region.id) || region.rect.isEmpty() ||
            !canvas.contains(region.rect))
            return fail(QStringLiteral("Некорректная область разбиения."));
        ids.insert(region.id);
        area += qint64(region.rect.width()) * region.rect.height();
        for (int j = 0; j < i; ++j)
            if (region.rect.intersects(regions[j].rect))
                return fail(QStringLiteral("Области разбиения пересекаются."));
        for (const auto &mask : region.masks)
        {
            dataSize += mask.data.size();
            if (mask.rect.isEmpty() || !region.rect.contains(mask.rect) || !mask.color.isValid() ||
                mask.data.size() != qint64(mask.rect.width()) * mask.rect.height() || dataSize > maxCells)
                return fail(QStringLiteral("Повреждена маска статического изображения."));
            for (const auto value : mask.data)
                if (value < 0 || value > 7)
                    return fail(QStringLiteral("Маска должна содержать уровни 0…7."));
        }
    }
    if (area != qint64(size.width()) * size.height())
        return fail(QStringLiteral("Разбиение не покрывает исходное изображение."));
    return true;
}

QList<int> StaticImageDocument::exportedRegions() const
{
    QList<int> indexes;
    for (int i = 0; i < regions.size(); ++i)
        if (!regions[i].excluded && !regions[i].masks.isEmpty())
            indexes.append(i);
    return indexes;
}

qint64 StaticImageDocument::cells() const
{
    qint64 result = 0;
    for (int i : exportedRegions())
        result += regions[i].cells();
    return result;
}

int StaticImageDocument::initCount() const
{
    int result = 0;
    for (int i : exportedRegions())
        result += regions[i].masks.size();
    return result;
}

QStringList StaticImageDocument::hardwareErrors() const
{
    QStringList errors;
    for (int i : exportedRegions())
    {
        const auto &region = regions[i];
        if (region.cells() > FpgaResourceLimits::staticPixels())
            errors.append(QStringLiteral("%1: %2/%3 ячеек памяти.")
                              .arg(region.name)
                              .arg(region.cells())
                              .arg(FpgaResourceLimits::staticPixels()));
        if (region.masks.size() > FpgaResourceLimits::staticParameters())
            errors.append(QStringLiteral("%1: %2/%3 init.")
                              .arg(region.name)
                              .arg(region.masks.size())
                              .arg(FpgaResourceLimits::staticParameters()));
        for (const auto &mask : region.masks)
            if (mask.rect.width() > 4095 || mask.rect.height() > 4095)
            {
                errors.append(QStringLiteral("%1: габарит маски превышает 4095 пикселей.").arg(region.name));
                break;
            }
    }
    return errors;
}

int StaticImageDocument::regionAt(const QPointF &point) const
{
    const QPoint pixel(int(std::floor(point.x())), int(std::floor(point.y())));
    for (int i = 0; i < regions.size(); ++i)
        if (regions[i].rect.contains(pixel))
            return i;
    return -1;
}

bool StaticImageDocument::split(int index, Qt::Orientation orientation, int coordinate)
{
    if (index < 0 || index >= regions.size() || regions.size() >= 4096)
        return false;
    const auto source = regions[index];
    QRect first = source.rect, second = source.rect;
    if (orientation == Qt::Vertical)
    {
        if (coordinate <= first.x() || coordinate >= first.x() + first.width())
            return false;
        first.setWidth(coordinate - first.x());
        second.setLeft(coordinate);
    }
    else
    {
        if (coordinate <= first.y() || coordinate >= first.y() + first.height())
            return false;
        first.setHeight(coordinate - first.y());
        second.setTop(coordinate);
    }
    StaticImageRegion a = source, b = source;
    a.rect = first;
    b.rect = second;
    b.id = nextId++;
    b.name = QStringLiteral("Часть %1").arg(b.id);
    a.masks.clear();
    b.masks.clear();
    for (const auto &mask : source.masks)
    {
        auto left = mask.clipped(first), right = mask.clipped(second);
        if (!left.rect.isEmpty())
            a.masks.append(left);
        if (!right.rect.isEmpty())
            b.masks.append(right);
    }
    regions[index] = a;
    regions.insert(index + 1, b);
    return true;
}

bool StaticImageDocument::mergeRegions(QList<int> indexes)
{
    std::sort(indexes.begin(), indexes.end());
    indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());
    if (indexes.size() < 2 || indexes.first() < 0 || indexes.last() >= regions.size())
        return false;
    QRect bounds;
    qint64 area = 0;
    for (int index : indexes)
    {
        bounds = bounds.united(regions[index].rect);
        area += qint64(regions[index].rect.width()) * regions[index].rect.height();
    }
    if (area != qint64(bounds.width()) * bounds.height())
        return false;
    StaticImageRegion merged = regions[indexes.first()];
    merged.rect = bounds;
    merged.masks.clear();
    merged.excluded = true;
    for (int index : indexes)
    {
        merged.masks += regions[index].masks;
        merged.excluded &= regions[index].excluded;
    }
    for (auto it = indexes.crbegin(); it != indexes.crend(); ++it)
        regions.removeAt(*it);
    regions.insert(indexes.first(), merged);
    return true;
}

bool StaticImageDocument::mergeMasks(int region, QList<int> indexes, const QColor &color)
{
    if (region < 0 || region >= regions.size() || !color.isValid())
        return false;
    auto &masks = regions[region].masks;
    std::sort(indexes.begin(), indexes.end());
    indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());
    if (indexes.size() < 2 || indexes.first() < 0 || indexes.last() >= masks.size())
        return false;
    QRect bounds;
    bool enabled = false;
    for (int index : indexes)
    {
        bounds = bounds.united(masks[index].rect);
        enabled |= masks[index].enabled;
    }
    if (qint64(bounds.width()) * bounds.height() > maxCells)
        return false;
    StaticImageMask merged{bounds, color, QByteArray(bounds.width() * bounds.height(), 0), enabled};
    for (int index : indexes)
    {
        const auto &mask = masks[index];
        for (int y = 0; y < mask.rect.height(); ++y)
            for (int x = 0; x < mask.rect.width(); ++x)
            {
                auto &destination = merged.data[(y + mask.rect.y() - bounds.y()) * bounds.width() + x +
                                                mask.rect.x() - bounds.x()];
                destination = qMax(destination, mask.data[y * mask.rect.width() + x]);
            }
    }
    const int destination = indexes.first();
    for (auto it = indexes.crbegin(); it != indexes.crend(); ++it)
        masks.removeAt(*it);
    masks.insert(destination, merged);
    return true;
}

void StaticImageDocument::excludeEmpty()
{
    for (auto &region : regions)
        if (region.empty())
            region.excluded = true;
}

QImage StaticImageDocument::render(int index, bool includeExcluded) const
{
    const QRect bounds = index < 0 ? QRect(QPoint(), size) : regions.value(index).rect;
    if (bounds.isEmpty())
        return {};
    QImage image(bounds.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    for (int i = 0; i < regions.size(); ++i)
    {
        if (index >= 0 && index != i)
            continue;
        const auto &region = regions[i];
        if (region.excluded && !includeExcluded)
            continue;
        // Как в STATICGROUP: первый совпавший прямоугольник выбирает маску, даже если её уровень 0.
        QImage covered(region.rect.size(), QImage::Format_Grayscale8);
        covered.fill(0);
        for (const auto &mask : region.masks)
        {
            if (!mask.enabled)
                continue;
            for (int y = 0; y < mask.rect.height(); ++y)
                for (int x = 0; x < mask.rect.width(); ++x)
                {
                    const QPoint local = mask.rect.topLeft() + QPoint(x, y) - region.rect.topLeft();
                    auto *row = covered.scanLine(local.y());
                    if (row[local.x()])
                        continue;
                    row[local.x()] = 1;
                    const int level = static_cast<quint8>(mask.data[y * mask.rect.width() + x]);
                    if (!level)
                        continue;
                    QColor color = mask.color;
                    color.setAlpha(qRound((level + 1) * 255.0 / 8.0));
                    image.setPixelColor(mask.rect.x() + x - bounds.x(), mask.rect.y() + y - bounds.y(),
                                        color);
                }
        }
    }
    return image;
}
