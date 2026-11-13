// ImageObject - редактируемый источник изображения до экспорта в растровую маску ПЛИС.

#include "ImageObject.h"
#include "fpga/compiler/FpgaResourceLimits.h"
#include "ProportionalResize.h"
#include "RasterGeometry.h"

#include <QBuffer>
#include <QHash>
#include <QPainter>
#include <QSvgRenderer>
#include <QTransform>
#include <QtMath>

#include <algorithm>
#include <limits>

namespace {
constexpr int kVisibleAlphaThreshold = 24;
constexpr int kColorBucket = 32;
constexpr int kMaxMaskLayers = 32;
constexpr int kMinLayerAlphaWeight = 1;
constexpr int kColorMergeDistanceSquared = 72 * 72;
constexpr int kWeakColorMergeDistanceSquared = 160 * 160;
constexpr int kWeakColorMergePercent = 75;
constexpr int kStaticGroupMaxDimension = 4095;

// ColorStats - взвешенная статистика исходных цветов для построения палитры.
struct ColorStats
{
    int firstIndex = 0;
    qint64 weight = 0;
    qint64 red = 0;
    qint64 green = 0;
    qint64 blue = 0;
};

// PaletteEntry - цвет палитры, его вес и порядок первого появления в изображении.
struct PaletteEntry
{
    QColor sourceColor;
    int firstIndex = 0;
    qint64 weight = 0;
};

// LayerAccumulator - границы и суммарная непрозрачность цветового слоя.
struct LayerAccumulator
{
    bool hasPixels = false;
    QRect bounds;
    qint64 alphaWeight = 0;
};

// ClassifiedImage - изображение, палитра и соответствие пикселей цветовым слоям.
struct ClassifiedImage
{
    QImage image;
    QList<PaletteEntry> palette;
    QVector<int> labels;
};

QRgb bucketColor(const QColor &color)
{
    return qRgb((color.red() / kColorBucket) * kColorBucket,
                (color.green() / kColorBucket) * kColorBucket,
                (color.blue() / kColorBucket) * kColorBucket);
}

QColor averageColor(const ColorStats &stats, QRgb fallback)
{
    if (stats.weight <= 0)
        return QColor(fallback);

    return QColor(qBound(0, static_cast<int>(stats.red / stats.weight), 255),
                  qBound(0, static_cast<int>(stats.green / stats.weight), 255),
                  qBound(0, static_cast<int>(stats.blue / stats.weight), 255));
}

void addPixelToLayer(LayerAccumulator &layer, int x, int y, int alpha)
{
    const QRect pixelRect(x, y, 1, 1);
    layer.bounds = layer.hasPixels ? layer.bounds.united(pixelRect) : pixelRect;
    layer.hasPixels = true;
    layer.alphaWeight += alpha;
}

void mergeClosePaletteEntry(PaletteEntry &target, const PaletteEntry &entry)
{
    const qint64 totalWeight = target.weight + entry.weight;
    target.sourceColor = QColor(
        qBound(0, static_cast<int>((target.sourceColor.red() * target.weight + entry.sourceColor.red() * entry.weight) / totalWeight), 255),
        qBound(0, static_cast<int>((target.sourceColor.green() * target.weight + entry.sourceColor.green() * entry.weight) / totalWeight), 255),
        qBound(0, static_cast<int>((target.sourceColor.blue() * target.weight + entry.sourceColor.blue() * entry.weight) / totalWeight), 255));
    target.firstIndex = qMin(target.firstIndex, entry.firstIndex);
    target.weight = totalWeight;
}

void absorbWeakPaletteEntry(PaletteEntry &target, const PaletteEntry &entry)
{
    target.firstIndex = qMin(target.firstIndex, entry.firstIndex);
    target.weight += entry.weight;
}

int colorDistanceSquared(const QColor &a, const QColor &b)
{
    const int dr = a.red() - b.red();
    const int dg = a.green() - b.green();
    const int db = a.blue() - b.blue();
    return dr * dr + dg * dg + db * db;
}

QList<PaletteEntry> paletteFromStoredLayers(const QList<ImageColorLayer> &storedLayers)
{
    QList<PaletteEntry> palette;
    palette.reserve(qMin(kMaxMaskLayers, storedLayers.size()));

    for (const ImageColorLayer &layer : storedLayers) {
        if (!layer.sourceColor.isValid())
            continue;
        // У статического изображения несколько независимых областей могут иметь один исходный цвет.
        if (std::any_of(palette.cbegin(), palette.cend(), [&layer](const PaletteEntry &entry) {
            return entry.sourceColor == layer.sourceColor;
        })) continue;
        PaletteEntry entry;
        entry.sourceColor = layer.sourceColor;
        entry.firstIndex = palette.size();
        entry.weight = 1;
        palette.append(entry);
        if (palette.size() >= kMaxMaskLayers)
            break;
    }

    return palette;
}

QList<PaletteEntry> mergeWeakAntialiasColors(const QList<PaletteEntry> &entries)
{
    if (entries.size() < 2)
        return entries;

    // Сглаживание SVG и растра создает промежуточные цвета на границах заливок.
    // Основные цвета сохраняются отдельно, а близкие промежуточные оттенки объединяются с ними.
    QList<PaletteEntry> weighted = entries;
    std::sort(weighted.begin(), weighted.end(), [](const PaletteEntry &a, const PaletteEntry &b) {
        if (a.weight != b.weight)
            return a.weight > b.weight;
        return a.firstIndex < b.firstIndex;
    });

    QList<PaletteEntry> dominant;
    dominant.reserve(weighted.size());
    for (const PaletteEntry &entry : weighted) {
        int nearestIndex = -1;
        int nearestDistance = 195075;
        for (int i = 0; i < dominant.size(); ++i) {
            const int distance = colorDistanceSquared(entry.sourceColor, dominant[i].sourceColor);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestIndex = i;
            }
        }

        if (nearestIndex >= 0 && nearestDistance <= kWeakColorMergeDistanceSquared) {
            PaletteEntry &nearest = dominant[nearestIndex];
            if (entry.weight * 100 <= nearest.weight * kWeakColorMergePercent) {
                absorbWeakPaletteEntry(nearest, entry);
                continue;
            }
        }

        dominant.append(entry);
    }

    std::sort(dominant.begin(), dominant.end(), [](const PaletteEntry &a, const PaletteEntry &b) {
        return a.firstIndex < b.firstIndex;
    });

    return dominant;
}

QHash<QRgb, ColorStats> collectColorStats(const QImage &image)
{
    QHash<QRgb, ColorStats> statsByBucket;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() < kVisibleAlphaThreshold)
                continue;

            const QRgb bucket = bucketColor(pixel);
            ColorStats stats = statsByBucket.value(bucket);
            if (stats.weight == 0)
                stats.firstIndex = y * image.width() + x;
            stats.weight += pixel.alpha();
            stats.red += pixel.red() * pixel.alpha();
            stats.green += pixel.green() * pixel.alpha();
            stats.blue += pixel.blue() * pixel.alpha();
            statsByBucket.insert(bucket, stats);
        }
    }
    return statsByBucket;
}

QList<PaletteEntry> paletteFromBuckets(const QHash<QRgb, ColorStats> &statsByBucket)
{
    QList<PaletteEntry> entries;
    entries.reserve(statsByBucket.size());

    for (auto it = statsByBucket.constBegin(); it != statsByBucket.constEnd(); ++it) {
        const ColorStats stats = it.value();
        if (stats.weight < kMinLayerAlphaWeight)
            continue;

        PaletteEntry entry;
        entry.sourceColor = averageColor(stats, it.key());
        entry.firstIndex = stats.firstIndex;
        entry.weight = stats.weight;
        entries.append(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const PaletteEntry &a, const PaletteEntry &b) {
        return a.firstIndex < b.firstIndex;
    });

    QList<PaletteEntry> clustered;
    clustered.reserve(entries.size());
    for (const PaletteEntry &entry : entries) {
        int nearestIndex = -1;
        int nearestDistance = 195075;
        for (int i = 0; i < clustered.size(); ++i) {
            const int distance = colorDistanceSquared(entry.sourceColor, clustered[i].sourceColor);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestIndex = i;
            }
        }

        if (nearestIndex >= 0 && nearestDistance <= kColorMergeDistanceSquared) {
            mergeClosePaletteEntry(clustered[nearestIndex], entry);
        } else {
            clustered.append(entry);
        }
    }

    entries = mergeWeakAntialiasColors(clustered);

    std::sort(entries.begin(), entries.end(), [](const PaletteEntry &a, const PaletteEntry &b) {
        return a.firstIndex < b.firstIndex;
    });

    if (entries.size() <= kMaxMaskLayers)
        return entries;

    QList<int> selectedIndexes;
    int heaviestIndex = 0;
    for (int i = 1; i < entries.size(); ++i) {
        if (entries[i].weight > entries[heaviestIndex].weight)
            heaviestIndex = i;
    }
    selectedIndexes.append(heaviestIndex);

    while (selectedIndexes.size() < kMaxMaskLayers) {
        int bestIndex = -1;
        qint64 bestScore = -1;

        for (int i = 0; i < entries.size(); ++i) {
            if (selectedIndexes.contains(i))
                continue;

            int minDistance = 195075;
            for (int selectedIndex : selectedIndexes) {
                minDistance = qMin(minDistance,
                                   colorDistanceSquared(entries[i].sourceColor,
                                                        entries[selectedIndex].sourceColor));
            }

            const qint64 score = entries[i].weight * qMax(1, minDistance);
            if (score > bestScore) {
                bestScore = score;
                bestIndex = i;
            }
        }

        if (bestIndex < 0)
            break;
        selectedIndexes.append(bestIndex);
    }

    QList<PaletteEntry> palette;
    palette.reserve(selectedIndexes.size());
    for (int index : selectedIndexes)
        palette.append(entries[index]);

    std::sort(palette.begin(), palette.end(), [](const PaletteEntry &a, const PaletteEntry &b) {
        return a.firstIndex < b.firstIndex;
    });

    return palette;
}

int nearestPaletteIndex(const QColor &pixel, const QList<PaletteEntry> &palette)
{
    int bestIndex = 0;
    int bestDistance = colorDistanceSquared(pixel, palette.first().sourceColor);

    for (int i = 1; i < palette.size(); ++i) {
        const int distance = colorDistanceSquared(pixel, palette[i].sourceColor);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex;
}

QColor maskColorForLayer(int layerIndex,
                         const QList<PaletteEntry> &palette,
                         const QList<ImageColorLayer> &storedLayers)
{
    const QColor autoColor = palette[layerIndex].sourceColor;
    if (layerIndex >= storedLayers.size())
        return autoColor;

    const ImageColorLayer &stored = storedLayers[layerIndex];
    return (!stored.autoMaskColor && stored.maskColor.isValid()) ? stored.maskColor : autoColor;
}

ClassifiedImage classifyImage(const QImage &source, const QList<ImageColorLayer> &storedLayers)
{
    ClassifiedImage classified;
    if (source.isNull())
        return classified;

    classified.image = source.convertToFormat(QImage::Format_ARGB32);
    classified.palette = paletteFromStoredLayers(storedLayers);
    if (classified.palette.isEmpty())
        classified.palette = paletteFromBuckets(collectColorStats(classified.image));
    if (classified.palette.isEmpty())
        return classified;

    const int width = classified.image.width();
    const int height = classified.image.height();
    classified.labels.fill(-1, width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const QColor pixel = classified.image.pixelColor(x, y);
            if (pixel.alpha() < kVisibleAlphaThreshold)
                continue;

            classified.labels[y * width + x] = nearestPaletteIndex(pixel, classified.palette);
        }
    }

    return classified;
}

QList<ImageMaskComponent> buildComponentsFromPixels(const ClassifiedImage &classified,
                                                    const QList<ImageColorLayer> &storedLayers,
                                                    const QVector<int> &pixels,
                                                    const QVector<LayerAccumulator> &layers)
{
    QList<ImageMaskComponent> components;
    if (classified.image.isNull() || classified.palette.isEmpty())
        return components;

    QVector<int> componentIndexByLayer(classified.palette.size(), -1);
    for (int layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
        const LayerAccumulator &layer = layers[layerIndex];
        if (!layer.hasPixels || layer.alphaWeight < kMinLayerAlphaWeight)
            continue;

        ImageMaskComponent component;
        component.layerIndex = layerIndex;
        component.bounds = layer.bounds;
        component.color = maskColorForLayer(layerIndex, classified.palette, storedLayers);
        component.mask = QImage(layer.bounds.size(), QImage::Format_ARGB32);
        component.mask.fill(Qt::transparent);
        componentIndexByLayer[layerIndex] = components.size();
        components.append(component);
    }

    const int width = classified.image.width();
    for (int index : pixels) {
        const int layerIndex = classified.labels[index];
        if (layerIndex < 0 || layerIndex >= componentIndexByLayer.size())
            continue;

        const int componentIndex = componentIndexByLayer[layerIndex];
        if (componentIndex < 0)
            continue;

        ImageMaskComponent &component = components[componentIndex];
        const int x = index % width;
        const int y = index / width;
        QColor maskPixel = component.color;
        maskPixel.setAlpha(classified.image.pixelColor(x, y).alpha());
        component.mask.setPixelColor(x - component.bounds.left(), y - component.bounds.top(), maskPixel);
    }

    return components;
}

QList<ImageMaskComponent> extractComponents(const QImage &source, const QList<ImageColorLayer> &storedLayers,
                                          bool splitConnected = false)
{
    QList<ImageMaskComponent> components;
    ClassifiedImage classified = classifyImage(source, storedLayers);
    if (classified.image.isNull() || classified.palette.isEmpty())
        return components;

    const int width = classified.image.width();
    const int height = classified.image.height();

    if (splitConnected) {
        // Палитра и сглаживание остаются прежними. Разделяем уже классифицированные
        // пиксели по восьмисвязным областям, сохраняя диагональные линии целыми.
        QVector<int> pixels;
        for (int seed = 0; seed < classified.labels.size(); ++seed) {
            const int colorIndex = classified.labels[seed];
            if (colorIndex < 0) continue;
            pixels.clear();
            pixels.append(seed);
            classified.labels[seed] = -1;
            LayerAccumulator region;
            for (qsizetype head = 0; head < pixels.size(); ++head) {
                const int pixel = pixels[head], x = pixel % width, y = pixel / width;
                addPixelToLayer(region, x, y, classified.image.pixelColor(x, y).alpha());
                for (int ny = qMax(0, y - 1); ny <= qMin(height - 1, y + 1); ++ny)
                    for (int nx = qMax(0, x - 1); nx <= qMin(width - 1, x + 1); ++nx) {
                        const int next = ny * width + nx;
                        if (classified.labels[next] != colorIndex) continue;
                        classified.labels[next] = -1;
                        pixels.append(next);
                    }
            }
            ImageMaskComponent component;
            component.layerIndex = components.size();
            component.bounds = region.bounds;
            component.color = classified.palette[colorIndex].sourceColor;
            component.mask = QImage(region.bounds.size(), QImage::Format_ARGB32);
            component.mask.fill(Qt::transparent);
            for (int pixel : pixels) {
                QColor color = component.color;
                color.setAlpha(classified.image.pixelColor(pixel % width, pixel / width).alpha());
                component.mask.setPixelColor(pixel % width - region.bounds.x(), pixel / width - region.bounds.y(), color);
            }
            components.append(component);
        }
        return components;
    }

    QVector<int> pixels;
    pixels.reserve(width * height);
    QVector<LayerAccumulator> layers(classified.palette.size());

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixelIndex = y * width + x;
            const int layerIndex = classified.labels[pixelIndex];
            if (layerIndex < 0)
                continue;

            const int alpha = classified.image.pixelColor(x, y).alpha();
            addPixelToLayer(layers[layerIndex], x, y, alpha);
            pixels.append(pixelIndex);
        }
    }

    return buildComponentsFromPixels(classified, storedLayers, pixels, layers);
}

QRectF relativeBounds(const QRect &bounds, const QSize &size)
{
    return QRectF(double(bounds.x()) / size.width(), double(bounds.y()) / size.height(),
                  double(bounds.width()) / size.width(), double(bounds.height()) / size.height());
}

void sortComponents(QList<ImageMaskComponent> &components, const QList<ImageColorLayer> &layers)
{
    std::stable_sort(components.begin(), components.end(), [&layers](const auto &a, const auto &b) {
        return layers.value(a.layerIndex).order < layers.value(b.layerIndex).order;
    });
}

qint64 staticGroupPixelCost(const QList<ImageMaskComponent> &components)
{
    qint64 pixels = 0;
    for (const ImageMaskComponent &component : components)
        pixels += qMax<qint64>(1, static_cast<qint64>(component.bounds.width()) * component.bounds.height());
    return pixels;
}

double staticGroupSimplificationScale(const QList<ImageMaskComponent> &components)
{
    double scale = 1.0;
    for (const ImageMaskComponent &component : components) {
        if (component.bounds.width() > kStaticGroupMaxDimension)
            scale = qMin(scale, static_cast<double>(kStaticGroupMaxDimension) / component.bounds.width());
        if (component.bounds.height() > kStaticGroupMaxDimension)
            scale = qMin(scale, static_cast<double>(kStaticGroupMaxDimension) / component.bounds.height());
    }

    const qint64 pixels = staticGroupPixelCost(components);
    if (pixels > FpgaResourceLimits::staticPixels())
        scale = qMin(scale, qSqrt(static_cast<double>(FpgaResourceLimits::staticPixels()) / pixels));

    return scale;
}

int rotationTileCost(const QSize &size)
{
    if (size.width() <= 0 || size.height() <= 0)
        return 0;
    return ((size.width() + 7) / 8) * ((size.height() + 7) / 8);
}

double rotationSimplificationScale(const QSize &size)
{
    if (size.width() <= 0 || size.height() <= 0)
        return 1.0;

    constexpr int maxTiles = 2048;
    double scale = 1.0;
    if ((size.width() + 7) / 8 > 255)
        scale = qMin(scale, (255.0 * 8.0) / size.width());

    const int tiles = rotationTileCost(size);
    if (tiles > maxTiles)
        scale = qMin(scale, qSqrt(static_cast<double>(maxTiles) / tiles));

    return scale;
}


}

ImageObject::ImageObject(Kind kind, QObject *parent) : BaseObject(parent), m_kind(kind) {}

void ImageObject::setRasterImage(const QImage &image, const QString &name)
{
    m_restoredComponents.clear();
    m_hasRestoredRotation = false;
    m_sourceImage = image.convertToFormat(QImage::Format_ARGB32);
    sourceName = name;
    format = QStringLiteral("raster");
    width = qMax(1, m_sourceImage.width());
    height = qMax(1, m_sourceImage.height());

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    buffer.open(QIODevice::WriteOnly);
    m_sourceImage.save(&buffer, "PNG");
    m_sourceBytes = pngBytes;
    rebuildColorLayers();
}

void ImageObject::setSvgData(const QByteArray &data, const QString &name, const QSize &defaultSize)
{
    m_restoredComponents.clear();
    m_hasRestoredRotation = false;
    m_sourceBytes = data;
    m_sourceImage = QImage();
    sourceName = name;
    format = QStringLiteral("svg");
    width = qMax(1, defaultSize.width());
    height = qMax(1, defaultSize.height());
    rebuildColorLayers();
}

QImage ImageObject::renderedSourceImage() const
{
    const int outW = qMax(1, qRound(width));
    const int outH = qMax(1, qRound(height));
    QImage image(outW, outH, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    if (format == QStringLiteral("svg")) {
        QSvgRenderer renderer(m_sourceBytes);
        QPainter painter(&image);
        renderer.render(&painter, QRectF(0, 0, outW, outH));
        return image.mirrored(mirrorHorizontal, mirrorVertical);
    }

    if (!m_sourceImage.isNull()) {
        return m_sourceImage.scaled(outW, outH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).mirrored(mirrorHorizontal, mirrorVertical);
    }

    return image;
}

QImage ImageObject::renderedImage() const
{
    const QImage source = renderedSourceImage();
    if (source.isNull())
        return source;

    const QList<ImageMaskComponent> components = maskComponents();
    if (components.isEmpty())
        return source;

    QImage colored(source.size(), QImage::Format_ARGB32);
    colored.fill(Qt::transparent);
    for (const ImageMaskComponent &component : components) {
        for (int y = 0; y < component.mask.height(); ++y) {
            for (int x = 0; x < component.mask.width(); ++x) {
                const QColor pixel = component.mask.pixelColor(x, y);
                if (pixel.alpha() <= 0)
                    continue;
                colored.setPixelColor(component.bounds.left() + x, component.bounds.top() + y, pixel);
            }
        }
    }
    return colored;
}

QColor ImageObject::effectiveMaskColor() const
{
    const QList<ImageMaskComponent> components = maskComponents();
    if (!components.isEmpty())
        return components.first().color;

    if (!autoMaskColor && maskColor.isValid())
        return maskColor;

    const QImage image = renderedSourceImage().convertToFormat(QImage::Format_ARGB32);
    if (image.isNull())
        return maskColor.isValid() ? maskColor : QColor(Qt::white);

    QHash<QRgb, int> weights;
    QRgb bestRgb = qRgb(maskColor.red(), maskColor.green(), maskColor.blue());
    int bestWeight = -1;

    const int step = qMax(1, qMin(image.width(), image.height()) / 96);
    for (int y = 0; y < image.height(); y += step) {
        for (int x = 0; x < image.width(); x += step) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() < 24)
                continue;

            const QRgb bucket = qRgb((pixel.red() / 8) * 8, (pixel.green() / 8) * 8, (pixel.blue() / 8) * 8);
            const int weight = weights.value(bucket) + pixel.alpha();
            weights.insert(bucket, weight);
            if (weight > bestWeight) {
                bestWeight = weight;
                bestRgb = bucket;
            }
        }
    }

    return QColor(bestRgb);
}

QList<ImageMaskComponent> ImageObject::maskComponents() const
{
    if (!m_restoredComponents.isEmpty()) {
        auto components = m_restoredComponents;
        const double sx = width / m_restoredSize.width(), sy = height / m_restoredSize.height();
        for (int i = 0; i < components.size(); ++i) {
            auto &c = components[i];
            const int outWidth = qMax(1, qRound(width)), outHeight = qMax(1, qRound(height));
            const int left = qBound(0, qRound(c.bounds.x() * sx), outWidth - 1);
            const int top = qBound(0, qRound(c.bounds.y() * sy), outHeight - 1);
            const int right = qBound(left + 1, qRound((c.bounds.x() + c.bounds.width()) * sx), outWidth);
            const int bottom = qBound(top + 1, qRound((c.bounds.y() + c.bounds.height()) * sy), outHeight);
            c.bounds = QRect(left, top, right - left, bottom - top);
            c.mask = c.mask.scaled(c.bounds.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
            if (mirrorHorizontal) c.bounds.moveLeft(outWidth - c.bounds.x() - c.bounds.width());
            if (mirrorVertical) c.bounds.moveTop(outHeight - c.bounds.y() - c.bounds.height());
            c.mask = c.mask.mirrored(mirrorHorizontal, mirrorVertical);
            c.layerIndex = i;
            if (i < m_colorLayers.size() && !m_colorLayers[i].autoMaskColor) c.color = m_colorLayers[i].maskColor;
            for (int y = 0; y < c.mask.height(); ++y)
                for (int x = 0; x < c.mask.width(); ++x) {
                    QColor pixel = c.color; pixel.setAlpha(c.mask.pixelColor(x, y).alpha());
                    c.mask.setPixelColor(x, y, pixel);
                }
        }
        if (!isRotationObject()) sortComponents(components, m_colorLayers);
        return components;
    }
    if (isRotationObject()) return extractComponents(renderedSourceImage(), m_colorLayers);

    // Идентифицируем области до отражения, чтобы цвет и позиция следовали за областью.
    const QImage source = renderedSourceImage().mirrored(mirrorHorizontal, mirrorVertical);
    const auto regions = extractComponents(source, m_colorLayers, true);
    QList<ImageMaskComponent> components;
    QMap<int, int> componentByLayer;
    for (auto region : regions) {
        const QRectF bounds = relativeBounds(region.bounds, source.size());
        int layerIndex = -1;
        double bestDistance = std::numeric_limits<double>::max();
        for (int i = 0; i < m_colorLayers.size(); ++i) {
            const auto &layer = m_colorLayers[i];
            if (layer.sourceColor != region.color) continue;
            const auto &reference = layer.sourceBounds;
            const double distance = qPow(bounds.left() - reference.left(), 2)
                + qPow(bounds.top() - reference.top(), 2) + qPow(bounds.right() - reference.right(), 2)
                + qPow(bounds.bottom() - reference.bottom(), 2);
            if (distance < bestDistance || (distance == bestDistance && componentByLayer.contains(layerIndex)
                && !componentByLayer.contains(i))) { bestDistance = distance; layerIndex = i; }
        }
        region.layerIndex = layerIndex >= 0 ? layerIndex : region.layerIndex;
        if (layerIndex >= 0 && !m_colorLayers[layerIndex].autoMaskColor)
            region.color = m_colorLayers[layerIndex].maskColor;
        for (int y = 0; y < region.mask.height(); ++y)
            for (int x = 0; x < region.mask.width(); ++x) {
                QColor color = region.color;
                color.setAlpha(region.mask.pixelColor(x, y).alpha());
                region.mask.setPixelColor(x, y, color);
            }
        // При изменении размера связная область может распасться на несколько частей.
        // Они сохраняют общие настройки исходного блока.
        if (componentByLayer.contains(region.layerIndex)) {
            auto &target = components[componentByLayer.value(region.layerIndex)];
            const QRect united = target.bounds.united(region.bounds);
            QImage mask(united.size(), QImage::Format_ARGB32); mask.fill(Qt::transparent);
            QPainter painter(&mask);
            painter.drawImage(target.bounds.topLeft() - united.topLeft(), target.mask);
            painter.drawImage(region.bounds.topLeft() - united.topLeft(), region.mask);
            painter.end();
            target.bounds = united; target.mask = mask;
        } else {
            componentByLayer.insert(region.layerIndex, components.size());
            components.append(region);
        }
    }
    for (auto &component : components) {
        if (mirrorHorizontal) component.bounds.moveLeft(source.width() - component.bounds.right() - 1);
        if (mirrorVertical) component.bounds.moveTop(source.height() - component.bounds.bottom() - 1);
        component.mask = component.mask.mirrored(mirrorHorizontal, mirrorVertical);
    }
    sortComponents(components, m_colorLayers);
    return components;
}

void ImageObject::restoreRasterComponents(const QList<ImageMaskComponent> &components, const QSize &size)
{
    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    for (const auto &c : components) painter.drawImage(c.bounds.topLeft(), c.mask);
    painter.end();
    setRasterImage(image, {});
    m_restoredComponents = components;
    m_restoredSize = size;
    m_colorLayers.clear();
    for (const auto &c : components)
        m_colorLayers.append({c.color, c.color, true, relativeBounds(c.bounds, size), int(m_colorLayers.size())});
}

void ImageObject::copyRasterSource(const ImageObject &source)
{
    mirrorHorizontal = source.mirrorHorizontal;
    mirrorVertical = source.mirrorVertical;
    m_sourceImage = source.m_sourceImage;
    m_sourceBytes = source.m_sourceBytes;
    m_restoredComponents = source.m_restoredComponents;
    m_restoredSize = source.m_restoredSize;
    m_restoredRotation = source.m_restoredRotation;
    m_restoredAngle = source.m_restoredAngle;
    m_hasRestoredRotation = source.m_hasRestoredRotation;
}

void ImageObject::restoreRotationCoefficients(int sine, int cosine)
{
    m_restoredRotation = QPoint(sine, cosine);
    m_restoredAngle = qRadiansToDegrees(qAtan2(double(sine), double(cosine)));
    rotationDegrees = m_restoredAngle;
    m_hasRestoredRotation = true;
}

QPoint ImageObject::rotationCoefficients() const
{
    if (m_hasRestoredRotation && rotationDegrees == m_restoredAngle) return m_restoredRotation;
    const double angle = qDegreesToRadians(rotationDegrees);
    return QPoint(qRound(qSin(angle) * 65536.0), qRound(qCos(angle) * 65536.0));
}

QList<ImageColorLayer> ImageObject::colorLayers() const
{
    return m_colorLayers;
}

void ImageObject::setColorLayers(const QList<ImageColorLayer> &layers)
{
    m_colorLayers = layers;
    // Старые проекты хранили настройки на цвет. Переносим их на каждую область
    // того же исходного цвета, не теряя пользовательскую перекраску.
    if (!isRotationObject() && m_restoredComponents.isEmpty()
        && (layers.isEmpty() || std::any_of(layers.cbegin(), layers.cend(), [](const auto &layer) { return layer.sourceBounds.isEmpty(); }))) {
        const QImage source = renderedSourceImage().mirrored(mirrorHorizontal, mirrorVertical);
        const auto components = extractComponents(source, layers, true);
        m_colorLayers.clear();
        for (const auto &component : components) {
            ImageColorLayer layer{component.color, component.color, true};
            for (const auto &stored : layers)
                if (stored.sourceColor == component.color) { layer = stored; break; }
            layer.sourceBounds = relativeBounds(component.bounds, source.size());
            layer.order = m_colorLayers.size();
            m_colorLayers.append(layer);
        }
    }
    for (int i = 0; i < m_colorLayers.size(); ++i)
        if (m_colorLayers[i].order < 0) m_colorLayers[i].order = i;
    if (!m_colorLayers.isEmpty()) {
        maskColor = m_colorLayers.first().maskColor;
        autoMaskColor = m_colorLayers.first().autoMaskColor;
    }
}

int ImageObject::layerPosition(int layerIndex) const
{
    return layerIndex >= 0 && layerIndex < m_colorLayers.size() ? m_colorLayers[layerIndex].order + 1 : 0;
}

bool ImageObject::hasRotation() const
{
    return isRotationObject() && qAbs(rotationDegrees) > 0.01;
}

QString ImageObject::xmlExportSummary() const
{
    if (isRotationObject()) {
        const auto raster = renderedSourceImage();
        const auto bounds = rasterAlphaBounds(raster);
        const QSize size = bounds.isEmpty() ? raster.size() : bounds.size();
        const double scale = rotationSimplificationScale(size);
        const QSize exportSize(scale >= 0.999
                                   ? size
                                   : QSize(qMax(1, static_cast<int>(qFloor(size.width() * scale))),
                                           qMax(1, static_cast<int>(qFloor(size.height() * scale)))));
        return QStringLiteral("rotationobject, упрощение: %1 (1 объект, память %2/2048 тайлов)")
            .arg(scale < 0.999 ? QStringLiteral("да") : QStringLiteral("нет"))
            .arg(rotationTileCost(exportSize));
    }

    QList<ImageMaskComponent> components = maskComponents();
    if (components.isEmpty()) {
        ImageMaskComponent component;
        component.bounds = QRect(0, 0, qMax(1, qRound(width)), qMax(1, qRound(height)));
        component.color = effectiveMaskColor();
        component.mask = renderedImage();
        components.append(component);
    }

    const double scale = staticGroupSimplificationScale(components);
    const qint64 sourcePixels = staticGroupPixelCost(components);
    const qint64 exportPixels = scale >= 0.999
        ? sourcePixels
        : qMin<qint64>(FpgaResourceLimits::staticPixels(), qMax<qint64>(1, qFloor(sourcePixels * scale * scale)));

    return QStringLiteral("staticgroup, упрощение: %1 (1 объект, %2 init, память %3/%4)")
        .arg(scale < 0.999 ? QStringLiteral("да") : QStringLiteral("нет"))
        .arg(qMax(1, components.size()))
        .arg(exportPixels)
        .arg(FpgaResourceLimits::staticPixels());
}

QByteArray ImageObject::sourcePayload() const
{
    return m_sourceBytes;
}

void ImageObject::setSourcePayload(const QByteArray &payload)
{
    m_restoredComponents.clear();
    m_hasRestoredRotation = false;
    m_sourceBytes = payload;
    if (format == QStringLiteral("svg")) {
        QSvgRenderer renderer(m_sourceBytes);
        const QSize size = renderer.defaultSize().isValid() ? renderer.defaultSize() : QSize(qRound(width), qRound(height));
        if (width <= 1.0)
            width = qMax(1, size.width());
        if (height <= 1.0)
            height = qMax(1, size.height());
        if (m_colorLayers.isEmpty())
            rebuildColorLayers();
        return;
    }

    m_sourceImage.loadFromData(m_sourceBytes, "PNG");
    if (!m_sourceImage.isNull()) {
        if (width <= 1.0)
            width = m_sourceImage.width();
        if (height <= 1.0)
            height = m_sourceImage.height();
        if (m_colorLayers.isEmpty())
            rebuildColorLayers();
    }
}

void ImageObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    Q_UNUSED(hexInit);
    Q_UNUSED(schema);
}

void ImageObject::draw(QPainter &painter)
{
    const QImage image = renderedImage();
    if (image.isNull())
        return;

    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (hasRotation()) {
        const QPointF center(x + width / 2.0, y + height / 2.0);
        painter.translate(center);
        painter.rotate(rotationDegrees);
        painter.translate(-center);
    }
    painter.drawImage(QRectF(x, y, width, height), image);
    painter.restore();
}

QString ImageObject::getTypeName() const
{
    return isRotationObject() ? QStringLiteral("rotationobject") : QStringLiteral("staticgroup");
}

QString ImageObject::getDisplayName() const
{
    return isRotationObject() ? QStringLiteral("Вращающееся изображение") : QStringLiteral("Статическое изображение");
}

QList<QPair<QString, QString>> ImageObject::getProperties() const
{
    QList<QPair<QString, QString>> props = {
        {"Источник", sourceName},
        {"Тип", format},
        {"X", QString::number(x)},
        {"Y", QString::number(y)},
        {"Ширина", QString::number(width)},
        {"Высота", QString::number(height)},
        {"Поворот (°)", QString::number(rotationDegrees, 'f', 2)},
        {"Автоцвет", autoMaskColor ? QStringLiteral("да") : QStringLiteral("нет")},
        {"Цвет маски", effectiveMaskColor().name()}
    };

    if (!isRotationObject()) {
        for (int i = props.size() - 1; i >= 0; --i)
            if (props[i].first == QStringLiteral("Поворот (°)")) props.removeAt(i);
    }
    const QList<ImageMaskComponent> components = maskComponents();
    if ((!isRotationObject() && !components.isEmpty()) || components.size() > 1) {
        props.removeLast();
        props.removeLast();
        for (const ImageMaskComponent &component : components) {
            if (!isRotationObject())
                props.append({QStringLiteral("Слой %1: Порядок").arg(component.layerIndex + 1), QString::number(layerPosition(component.layerIndex))});
            props.append({QStringLiteral("Слой %1: Цвет").arg(component.layerIndex + 1), component.color.name()});
        }
    }

    props.append({QStringLiteral("XML экспорт"), xmlExportSummary()});
    return props;
}

QRectF ImageObject::getBoundingRect() const
{
    return QRectF(x, y, width, height);
}

bool ImageObject::contains(const QPointF &point) const
{
    const QRectF baseRect(x, y, width, height);
    if (!hasRotation())
        return baseRect.contains(point);

    const QPointF center = baseRect.center();
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(rotationDegrees);
    transform.translate(-center.x(), -center.y());
    const QTransform inverse = transform.inverted();
    return baseRect.contains(inverse.map(point));
}

QTransform ImageObject::rotationFrame() const
{
    const auto center = QPointF(x + width / 2, y + height / 2);
    QTransform frame; frame.translate(center.x(), center.y()); frame.rotate(rotationDegrees);
    frame.translate(-width / 2, -height / 2); frame.scale(width, height);
    return frame;
}

QPointF ImageObject::rotationCenter() const
{
    return editorRotationPivot() ? rotationFrame().map(*editorRotationPivot()) : QPointF(x + width / 2, y + height / 2);
}

void ImageObject::moveBy(double dx, double dy)
{
    x += dx;
    y += dy;
    emit changed();
}

void ImageObject::resizeBy(int edgeFlags, double dx, double dy, bool proportional)
{
    if (!canResize())
        return;

    if (proportional) {
        const auto rect = proportionalResizeRect(QRectF(x, y, width, height), edgeFlags, dx, dy).rect;
        x = rect.x(); y = rect.y(); width = rect.width(); height = rect.height();
        emit changed();
        return;
    }

    if (edgeFlags & 1) {
        x += dx;
        width -= dx;
    }
    if (edgeFlags & 2)
        width += dx;
    if (edgeFlags & 4) {
        y += dy;
        height -= dy;
    }
    if (edgeFlags & 8)
        height += dy;

    width = qMax(1.0, width);
    height = qMax(1.0, height);
    emit changed();
}

void ImageObject::setRotation(double angle)
{
    if (!isRotationObject() || !canResize())
        return;
    const auto pivot = rotationCenter();
    rotationDegrees = angle;
    if (editorRotationPivot()) {
        const auto shift = pivot - rotationCenter();
        x += shift.x(); y += shift.y();
    }
    emit changed();
}

bool ImageObject::setObjectProperty(const QString &name, const QString &value)
{
    bool ok = false;
    if (name == QStringLiteral("X")) {
        x = value.toDouble(&ok);
    } else if (name == QStringLiteral("Y")) {
        y = value.toDouble(&ok);
    } else if (name == QStringLiteral("Ширина")) {
        if (!canResize()) {
            setValidationMessage(QStringLiteral("Размер изображения заблокирован после экспорта."));
            return false;
        }
        width = qMax(1.0, value.toDouble(&ok));
    } else if (name == QStringLiteral("Высота")) {
        if (!canResize()) {
            setValidationMessage(QStringLiteral("Размер изображения заблокирован после экспорта."));
            return false;
        }
        height = qMax(1.0, value.toDouble(&ok));
    } else if (name == QStringLiteral("Поворот (°)")) {
        if (!isRotationObject())
            return false;
        if (!canResize()) {
            setValidationMessage(QStringLiteral("Поворот изображения заблокирован после экспорта."));
            return false;
        }
        const double angle = value.toDouble(&ok);
        if (ok) { setRotation(angle); return true; }
    } else if (name == QStringLiteral("Автоцвет")) {
        const QString normalized = value.trimmed().toLower();
        if (normalized == QStringLiteral("да") || normalized == QStringLiteral("yes")
            || normalized == QStringLiteral("true") || normalized == QStringLiteral("1")) {
            autoMaskColor = true;
            if (!m_colorLayers.isEmpty())
                m_colorLayers[0].autoMaskColor = true;
            ok = true;
        } else if (normalized == QStringLiteral("нет") || normalized == QStringLiteral("no")
                   || normalized == QStringLiteral("false") || normalized == QStringLiteral("0")) {
            autoMaskColor = false;
            if (!m_colorLayers.isEmpty())
                m_colorLayers[0].autoMaskColor = false;
            ok = true;
        }
    } else if (name == QStringLiteral("Цвет маски")) {
        const QColor color(value);
        ok = color.isValid();
        if (ok) {
            maskColor = color;
            autoMaskColor = false;
            if (m_colorLayers.isEmpty())
                rebuildColorLayers();
            if (!m_colorLayers.isEmpty()) {
                m_colorLayers[0].maskColor = color;
                m_colorLayers[0].autoMaskColor = false;
            }
        }
    } else if (!isRotationObject() && name.startsWith(QStringLiteral("Слой ")) && name.endsWith(QStringLiteral(": Порядок"))) {
        const int colon = name.indexOf(':');
        const int layer = name.mid(5, colon - 5).toInt(&ok) - 1;
        if (!ok || layer < 0 || layer >= m_colorLayers.size()) return false;
        const int position = value.toInt(&ok) - 1;
        if (!ok || position < 0 || position >= m_colorLayers.size()) return false;
        const int previous = m_colorLayers[layer].order;
        for (auto &other : m_colorLayers) {
            if (previous < position && other.order > previous && other.order <= position) --other.order;
            else if (position < previous && other.order >= position && other.order < previous) ++other.order;
        }
        m_colorLayers[layer].order = position;
    } else if (name.startsWith(QStringLiteral("Слой ")) && name.endsWith(QStringLiteral(": Цвет"))) {
        const int colonIndex = name.indexOf(':');
        const int layerNumber = name.mid(QStringLiteral("Слой ").size(), colonIndex - QStringLiteral("Слой ").size()).toInt(&ok);
        const QColor color(value);
        ok = ok && color.isValid();
        if (ok) {
            if (m_colorLayers.isEmpty())
                rebuildColorLayers();
            const int layerIndex = layerNumber - 1;
            if (layerIndex >= 0 && layerIndex < m_colorLayers.size()) {
                m_colorLayers[layerIndex].maskColor = color;
                m_colorLayers[layerIndex].autoMaskColor = false;
            } else {
                ok = false;
            }
        }
    }

    if (ok)
        emit changed();
    return ok;
}

void ImageObject::rebuildColorLayers()
{
    const auto source = renderedSourceImage().mirrored(mirrorHorizontal, mirrorVertical);
    const QList<ImageMaskComponent> components = extractComponents(source, {}, !isRotationObject());
    m_colorLayers.clear();
    m_colorLayers.reserve(components.size());
    for (const ImageMaskComponent &component : components) {
        ImageColorLayer layer;
        layer.sourceColor = component.color;
        layer.maskColor = component.color;
        layer.autoMaskColor = true;
        layer.sourceBounds = relativeBounds(component.bounds, source.size());
        layer.order = m_colorLayers.size();
        m_colorLayers.append(layer);
    }

    if (!m_colorLayers.isEmpty()) {
        maskColor = m_colorLayers.first().maskColor;
        autoMaskColor = m_colorLayers.first().autoMaskColor;
    }
}

QSharedPointer<BaseObject> ImageObject::clone() const
{
    auto *clone = new ImageObject(isRotationObject() ? ImageObject::Kind::Rotating : ImageObject::Kind::Static);
    clone->x = x;
    clone->y = y;
    clone->width = width;
    clone->height = height;
    clone->maskColor = maskColor;
    clone->autoMaskColor = autoMaskColor;
    clone->rotationDegrees = rotationDegrees;
    clone->sourceName = sourceName;
    clone->format = format;
    clone->copyRasterSource(*this);
    clone->setColorLayers(colorLayers());
    copyEditorStateTo(*clone);
    return QSharedPointer<BaseObject>(clone);
}
