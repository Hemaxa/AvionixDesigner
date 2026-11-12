#include "ImageResourceBuilder.h"

#include "fpga/compiler/FpgaResourceLimits.h"
#include "RasterGeometry.h"
#include <QtMath>
#include <limits>

namespace {
constexpr int kFallbackStaticGroupMaxDimension = 4095;
constexpr int kFallbackStaticGroupAddressablePixels = 65536;
constexpr int kFallbackRotationObjectAddressableTiles = 2048;
}

int ImageResourceBuilder::maxUnsignedFieldValue(const ParamSchema &schema, const QString &fieldName, int fallback)
{
    if (!schema.contains(fieldName))
        return fallback;

    const int size = schema[fieldName].size;
    if (size <= 0)
        return fallback;

    const qint64 value = size >= 31
        ? static_cast<qint64>(std::numeric_limits<int>::max())
        : ((static_cast<qint64>(1) << size) - 1);
    return static_cast<int>(qMin<qint64>(value, std::numeric_limits<int>::max()));
}

int ImageResourceBuilder::staticGroupAddressablePixels(const ParamSchema &schema)
{
    const int maxAddr = maxUnsignedFieldValue(schema, QStringLiteral("addr"), kFallbackStaticGroupAddressablePixels - 1);
    return qMin(FpgaResourceLimits::staticPixels(), qMax(1, maxAddr + 1));
}

QSizeF ImageResourceBuilder::fitSizeToStaticGroupLimits(const QSizeF &size, const ParamSchema &schema)
{
    if (size.width() <= 0.0 || size.height() <= 0.0)
        return QSizeF(1.0, 1.0);

    const int maxWidth = maxUnsignedFieldValue(schema, QStringLiteral("w"), kFallbackStaticGroupMaxDimension);
    const int maxHeight = maxUnsignedFieldValue(schema, QStringLiteral("h"), kFallbackStaticGroupMaxDimension);
    const int maxPixels = staticGroupAddressablePixels(schema);

    double scale = 1.0;
    if (size.width() > maxWidth)
        scale = qMin(scale, static_cast<double>(maxWidth) / size.width());
    if (size.height() > maxHeight)
        scale = qMin(scale, static_cast<double>(maxHeight) / size.height());
    if (size.width() * size.height() > maxPixels)
        scale = qMin(scale, qSqrt(static_cast<double>(maxPixels) / (size.width() * size.height())));

    return QSizeF(qMax(1.0, static_cast<double>(qFloor(size.width() * scale))),
                  qMax(1.0, static_cast<double>(qFloor(size.height() * scale))));
}

qint64 ImageResourceBuilder::staticGroupPixelCost(const QList<ImageMaskComponent> &components)
{
    qint64 pixels = 0;
    for (const ImageMaskComponent &component : components)
        pixels += qMax<qint64>(1, static_cast<qint64>(component.bounds.width()) * component.bounds.height());
    return pixels;
}

QList<ImageMaskComponent> ImageResourceBuilder::scaledStaticGroupComponents(const QList<ImageMaskComponent> &components,
                                                      double scale)
{
    QList<ImageMaskComponent> scaled;
    scaled.reserve(components.size());

    for (const ImageMaskComponent &component : components) {
        if (component.mask.isNull() || component.bounds.isEmpty())
            continue;

        ImageMaskComponent out;
        out.layerIndex = component.layerIndex;
        out.color = component.color;
        const int width = qMax(1, static_cast<int>(qFloor(component.bounds.width() * scale)));
        const int height = qMax(1, static_cast<int>(qFloor(component.bounds.height() * scale)));
        out.bounds = QRect(qRound(component.bounds.left() * scale),
                           qRound(component.bounds.top() * scale),
                           width,
                           height);
        out.mask = component.mask.scaled(width, height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        scaled.append(out);
    }

    return scaled;
}

QList<ImageMaskComponent> ImageResourceBuilder::simplifyComponentsForStaticGroup(const ParamSchema &schema,
                                                           const QList<ImageMaskComponent> &components)
{
    const int maxWidth = maxUnsignedFieldValue(schema, QStringLiteral("w"), kFallbackStaticGroupMaxDimension);
    const int maxHeight = maxUnsignedFieldValue(schema, QStringLiteral("h"), kFallbackStaticGroupMaxDimension);
    const int maxPixels = staticGroupAddressablePixels(schema);
    if (components.isEmpty())
        return {};

    double scale = 1.0;
    for (const ImageMaskComponent &component : components) {
        if (component.bounds.width() > maxWidth)
            scale = qMin(scale, static_cast<double>(maxWidth) / component.bounds.width());
        if (component.bounds.height() > maxHeight)
            scale = qMin(scale, static_cast<double>(maxHeight) / component.bounds.height());
    }

    const qint64 initialPixels = staticGroupPixelCost(components);
    if (initialPixels > maxPixels)
        scale = qMin(scale, qSqrt(static_cast<double>(maxPixels) / initialPixels));

    if (scale >= 0.999)
        return components;

    QList<ImageMaskComponent> simplified = scaledStaticGroupComponents(components, scale);
    for (int attempt = 0; attempt < 8 && staticGroupPixelCost(simplified) > maxPixels; ++attempt) {
        const qint64 currentPixels = staticGroupPixelCost(simplified);
        scale *= qSqrt(static_cast<double>(maxPixels) / currentPixels) * 0.98;
        simplified = scaledStaticGroupComponents(components, scale);
    }

    return simplified;
}

int ImageResourceBuilder::rotationTileCost(const QSize &size)
{
    if (size.width() <= 0 || size.height() <= 0)
        return 0;
    return ((size.width() + 7) / 8) * ((size.height() + 7) / 8);
}

ImageMaskComponent ImageResourceBuilder::singleComponentForRotationObject(const ImageObject *image)
{
    ImageMaskComponent component;
    if (!image)
        return component;

    const QImage source = image->renderedSourceImage().convertToFormat(QImage::Format_ARGB32);
    if (source.isNull())
        return component;

    QRect bounds = rasterAlphaBounds(source);
    if (bounds.isEmpty())
        bounds = QRect(0, 0, qMax(1, source.width()), qMax(1, source.height()));

    component.bounds = bounds;
    component.color = image->effectiveMaskColor();
    component.mask = source.copy(bounds);
    return component;
}

ImageMaskComponent ImageResourceBuilder::simplifyComponentForRotationObject(const ImageMaskComponent &component)
{
    if (component.mask.isNull() || component.bounds.isEmpty())
        return component;

    double scale = 1.0;
    if ((component.bounds.width() + 7) / 8 > 255)
        scale = qMin(scale, (255.0 * 8.0) / component.bounds.width());

    const int initialTiles = rotationTileCost(component.bounds.size());
    if (initialTiles > kFallbackRotationObjectAddressableTiles)
        scale = qMin(scale, qSqrt(static_cast<double>(kFallbackRotationObjectAddressableTiles) / initialTiles));

    if (scale >= 0.999)
        return component;

    ImageMaskComponent simplified = component;
    for (int attempt = 0; attempt < 8; ++attempt) {
        const int width = qMax(1, static_cast<int>(qFloor(component.bounds.width() * scale)));
        const int height = qMax(1, static_cast<int>(qFloor(component.bounds.height() * scale)));
        simplified.bounds = QRect(qRound(component.bounds.left() * scale),
                                  qRound(component.bounds.top() * scale),
                                  width,
                                  height);
        simplified.mask = component.mask.scaled(width, height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        if (rotationTileCost(simplified.bounds.size()) <= kFallbackRotationObjectAddressableTiles
            && (simplified.bounds.width() + 7) / 8 <= 255) {
            break;
        }
        scale *= 0.96;
    }

    return simplified;
}

