#include "ImageObject.h"

#include <QBuffer>
#include <QHash>
#include <QPainter>
#include <QSvgRenderer>
#include <QTransform>
#include <QtMath>

#include <algorithm>

namespace {
constexpr int kVisibleAlphaThreshold = 24;
constexpr int kColorBucket = 32;
constexpr int kMaxMaskLayers = 32;
constexpr int kMinLayerAlphaWeight = 1;
constexpr int kColorMergeDistanceSquared = 72 * 72;
constexpr int kWeakColorMergeDistanceSquared = 160 * 160;
constexpr int kWeakColorMergePercent = 75;
constexpr int kStaticGroupMaxDimension = 4095;
constexpr int kStaticGroupMaxPixels = 65536;

struct ColorStats
{
    int firstIndex = 0;
    qint64 weight = 0;
    qint64 red = 0;
    qint64 green = 0;
    qint64 blue = 0;
};

struct PaletteEntry
{
    QColor sourceColor;
    int firstIndex = 0;
    qint64 weight = 0;
};

struct LayerAccumulator
{
    bool hasPixels = false;
    QRect bounds;
    qint64 alphaWeight = 0;
};

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

    // SVG/raster antialiasing often creates dark edge colors between real fills.
    // Keep strong source colors separate, but fold weaker nearby colors into them.
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

QList<ImageMaskComponent> extractComponents(const QImage &source, const QList<ImageColorLayer> &storedLayers)
{
    QList<ImageMaskComponent> components;
    const ClassifiedImage classified = classifyImage(source, storedLayers);
    if (classified.image.isNull() || classified.palette.isEmpty())
        return components;

    const int width = classified.image.width();
    const int height = classified.image.height();

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

QList<QList<ImageMaskComponent>> extractComponentGroups(const QImage &source, const QList<ImageColorLayer> &storedLayers)
{
    QList<QList<ImageMaskComponent>> groups;
    const ClassifiedImage classified = classifyImage(source, storedLayers);
    if (classified.image.isNull() || classified.palette.isEmpty())
        return groups;

    const int width = classified.image.width();
    const int height = classified.image.height();
    const int pixelCount = width * height;
    QVector<quint8> visited(pixelCount, 0);

    constexpr int kNeighborCount = 8;
    constexpr int dx[kNeighborCount] = {-1, 0, 1, -1, 1, -1, 0, 1};
    constexpr int dy[kNeighborCount] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int startIndex = 0; startIndex < pixelCount; ++startIndex) {
        if (classified.labels[startIndex] < 0 || visited[startIndex])
            continue;

        QVector<int> queue;
        QVector<int> pixels;
        QVector<LayerAccumulator> layers(classified.palette.size());
        queue.reserve(256);
        pixels.reserve(256);

        visited[startIndex] = 1;
        queue.append(startIndex);

        for (int head = 0; head < queue.size(); ++head) {
            const int pixelIndex = queue[head];
            pixels.append(pixelIndex);

            const int x = pixelIndex % width;
            const int y = pixelIndex / width;
            const int layerIndex = classified.labels[pixelIndex];
            const int alpha = classified.image.pixelColor(x, y).alpha();
            addPixelToLayer(layers[layerIndex], x, y, alpha);

            for (int i = 0; i < kNeighborCount; ++i) {
                const int nx = x + dx[i];
                const int ny = y + dy[i];
                if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                    continue;

                const int neighborIndex = ny * width + nx;
                if (visited[neighborIndex] || classified.labels[neighborIndex] < 0)
                    continue;

                visited[neighborIndex] = 1;
                queue.append(neighborIndex);
            }
        }

        const QList<ImageMaskComponent> components = buildComponentsFromPixels(classified, storedLayers, pixels, layers);
        if (!components.isEmpty())
            groups.append(components);
    }

    return groups;
}

QRect visibleBoundsInRect(const QImage &image, const QRect &rect)
{
    QRect bounds;
    bool hasPixels = false;

    const QRect clipped = rect.intersected(image.rect());
    for (int y = clipped.top(); y <= clipped.bottom(); ++y) {
        for (int x = clipped.left(); x <= clipped.right(); ++x) {
            if (image.pixelColor(x, y).alpha() <= 0)
                continue;

            const QRect pixelRect(x, y, 1, 1);
            bounds = hasPixels ? bounds.united(pixelRect) : pixelRect;
            hasPixels = true;
        }
    }

    return hasPixels ? bounds : QRect();
}

QList<QRect> staticGroupExportTiles(const ImageMaskComponent &component)
{
    QList<QRect> tiles;
    if (component.mask.isNull() || component.bounds.isEmpty())
        return tiles;

    const int sourceWidth = component.mask.width();
    const int sourceHeight = component.mask.height();
    const int safeMaxWidth = qMax(1, qMin(kStaticGroupMaxDimension, kStaticGroupMaxPixels));

    for (int left = 0; left < sourceWidth; left += safeMaxWidth) {
        const int tileWidth = qMin(safeMaxWidth, sourceWidth - left);
        const int tileHeightLimit = qMax(1, qMin(kStaticGroupMaxDimension, kStaticGroupMaxPixels / qMax(1, tileWidth)));

        for (int top = 0; top < sourceHeight; top += tileHeightLimit) {
            const QRect candidate(left, top, tileWidth, qMin(tileHeightLimit, sourceHeight - top));
            const QRect visibleBounds = visibleBoundsInRect(component.mask, candidate);
            if (!visibleBounds.isEmpty())
                tiles.append(visibleBounds);
        }
    }

    return tiles;
}

int staticGroupExportObjectCount(const QList<ImageMaskComponent> &components, int *initCount)
{
    int objectCount = 0;
    int currentPixels = 0;
    int states = 0;

    for (const ImageMaskComponent &component : components) {
        const QList<QRect> tiles = staticGroupExportTiles(component);
        for (const QRect &tile : tiles) {
            const int tilePixels = qMax(1, tile.width() * tile.height());
            if (currentPixels > 0 && currentPixels + tilePixels > kStaticGroupMaxPixels) {
                ++objectCount;
                currentPixels = 0;
            }

            currentPixels += tilePixels;
            ++states;
        }
    }

    if (currentPixels > 0)
        ++objectCount;

    if (initCount)
        *initCount = states;
    return objectCount;
}

QString objectCountWord(int count)
{
    const int mod100 = count % 100;
    if (mod100 >= 11 && mod100 <= 14)
        return QStringLiteral("объектов");

    switch (count % 10) {
    case 1:
        return QStringLiteral("объект");
    case 2:
    case 3:
    case 4:
        return QStringLiteral("объекта");
    default:
        return QStringLiteral("объектов");
    }
}
}

ImageObject::ImageObject(QObject *parent) : BaseObject(parent) {}

void ImageObject::setRasterImage(const QImage &image, const QString &name)
{
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
        return image;
    }

    if (!m_sourceImage.isNull()) {
        return m_sourceImage.scaled(outW, outH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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
    return extractComponents(renderedSourceImage(), m_colorLayers);
}

QList<QList<ImageMaskComponent>> ImageObject::maskComponentGroups() const
{
    return extractComponentGroups(renderedSourceImage(), m_colorLayers);
}

QList<ImageColorLayer> ImageObject::colorLayers() const
{
    return m_colorLayers;
}

void ImageObject::setColorLayers(const QList<ImageColorLayer> &layers)
{
    m_colorLayers = layers;
    if (!m_colorLayers.isEmpty()) {
        maskColor = m_colorLayers.first().maskColor;
        autoMaskColor = m_colorLayers.first().autoMaskColor;
    }
}

bool ImageObject::hasRotation() const
{
    return qAbs(rotationDegrees) > 0.01;
}

QString ImageObject::xmlExportSummary() const
{
    QList<ImageMaskComponent> components = maskComponents();
    if (components.isEmpty()) {
        ImageMaskComponent component;
        component.bounds = QRect(0, 0, qMax(1, qRound(width)), qMax(1, qRound(height)));
        component.color = effectiveMaskColor();
        component.mask = renderedImage();
        components.append(component);
    }

    if (hasRotation()) {
        const int objectCount = qMax(1, components.size());
        return QStringLiteral("rotationobject, дробление: %1 (%2 %3)")
            .arg(objectCount > 1 ? QStringLiteral("да") : QStringLiteral("нет"))
            .arg(objectCount)
            .arg(objectCountWord(objectCount));
    }

    int initCount = 0;
    const int objectCount = staticGroupExportObjectCount(components, &initCount);
    return QStringLiteral("staticgroup, дробление: %1 (%2 %3, %4 init)")
        .arg(objectCount > 1 ? QStringLiteral("да") : QStringLiteral("нет"))
        .arg(qMax(1, objectCount))
        .arg(objectCountWord(qMax(1, objectCount)))
        .arg(qMax(1, initCount));
}

QByteArray ImageObject::sourcePayload() const
{
    return m_sourceBytes;
}

void ImageObject::setSourcePayload(const QByteArray &payload)
{
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
    return QStringLiteral("image");
}

QString ImageObject::getDisplayName() const
{
    return QStringLiteral("Изображение");
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

    const QList<ImageMaskComponent> components = maskComponents();
    if (components.size() > 1) {
        props.removeLast();
        props.removeLast();
        for (const ImageMaskComponent &component : components)
            props.append({QStringLiteral("Слой %1: Цвет").arg(component.layerIndex + 1), component.color.name()});
    }

    props.append({QStringLiteral("XML экспорт"), xmlExportSummary()});
    return props;
}

QRectF ImageObject::getBoundingRect() const
{
    const QRectF baseRect(x, y, width, height);
    if (!hasRotation())
        return baseRect;

    const QPointF center = baseRect.center();
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(rotationDegrees);
    transform.translate(-center.x(), -center.y());
    return transform.mapRect(baseRect);
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

void ImageObject::moveBy(double dx, double dy)
{
    x += dx;
    y += dy;
    emit changed();
}

void ImageObject::resizeBy(int edgeFlags, double dx, double dy)
{
    if (!canResize())
        return;

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
    rotationDegrees = angle;
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
        if (!canResize()) {
            setValidationMessage(QStringLiteral("Поворот изображения заблокирован после экспорта."));
            return false;
        }
        rotationDegrees = value.toDouble(&ok);
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
    const QList<ImageMaskComponent> components = extractComponents(renderedSourceImage(), {});
    m_colorLayers.clear();
    m_colorLayers.reserve(components.size());
    for (const ImageMaskComponent &component : components) {
        ImageColorLayer layer;
        layer.sourceColor = component.color;
        layer.maskColor = component.color;
        layer.autoMaskColor = true;
        m_colorLayers.append(layer);
    }

    if (!m_colorLayers.isEmpty()) {
        maskColor = m_colorLayers.first().maskColor;
        autoMaskColor = m_colorLayers.first().autoMaskColor;
    }
}
