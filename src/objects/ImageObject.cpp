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
constexpr int kColorBucket = 16;
constexpr int kMinComponentAlphaWeight = 255 * 3;

struct ColorStats
{
    int firstIndex = 0;
    qint64 weight = 0;
    qint64 red = 0;
    qint64 green = 0;
    qint64 blue = 0;
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

QList<ImageMaskComponent> extractComponents(const QImage &source, const QList<ImageColorLayer> &storedLayers)
{
    QList<ImageMaskComponent> components;
    if (source.isNull())
        return components;

    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    const int width = image.width();
    const int height = image.height();
    const int pixelCount = width * height;

    QHash<QRgb, ColorStats> statsByBucket;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() < kVisibleAlphaThreshold)
                continue;

            const QRgb bucket = bucketColor(pixel);
            ColorStats stats = statsByBucket.value(bucket);
            if (stats.weight == 0)
                stats.firstIndex = y * width + x;
            stats.weight += pixel.alpha();
            stats.red += pixel.red() * pixel.alpha();
            stats.green += pixel.green() * pixel.alpha();
            stats.blue += pixel.blue() * pixel.alpha();
            statsByBucket.insert(bucket, stats);
        }
    }

    QList<QRgb> buckets = statsByBucket.keys();
    std::sort(buckets.begin(), buckets.end(), [&statsByBucket](QRgb a, QRgb b) {
        return statsByBucket.value(a).firstIndex < statsByBucket.value(b).firstIndex;
    });

    QHash<QRgb, int> layerByBucket;
    QList<QColor> sourceColors;
    for (QRgb bucket : buckets) {
        const ColorStats stats = statsByBucket.value(bucket);
        if (stats.weight < kMinComponentAlphaWeight)
            continue;
        layerByBucket.insert(bucket, sourceColors.size());
        sourceColors.append(averageColor(stats, bucket));
    }

    QVector<int> labels(pixelCount, -1);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() < kVisibleAlphaThreshold)
                continue;

            const QRgb bucket = bucketColor(pixel);
            const auto it = layerByBucket.constFind(bucket);
            if (it != layerByBucket.constEnd())
                labels[y * width + x] = it.value();
        }
    }

    QVector<uchar> visited(pixelCount, 0);
    QVector<int> queue;
    queue.reserve(pixelCount);

    for (int start = 0; start < pixelCount; ++start) {
        const int layerIndex = labels[start];
        if (layerIndex < 0 || visited[start])
            continue;

        queue.clear();
        queue.append(start);
        visited[start] = 1;

        QVector<int> pixels;
        pixels.reserve(256);
        QRect bounds(start % width, start / width, 1, 1);
        qint64 alphaWeight = 0;

        for (int head = 0; head < queue.size(); ++head) {
            const int current = queue[head];
            pixels.append(current);

            const int cx = current % width;
            const int cy = current / width;
            bounds = bounds.united(QRect(cx, cy, 1, 1));
            alphaWeight += image.pixelColor(cx, cy).alpha();

            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    if (ox == 0 && oy == 0)
                        continue;

                    const int nx = cx + ox;
                    const int ny = cy + oy;
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                        continue;

                    const int ni = ny * width + nx;
                    if (visited[ni] || labels[ni] != layerIndex)
                        continue;

                    visited[ni] = 1;
                    queue.append(ni);
                }
            }
        }

        if (alphaWeight < kMinComponentAlphaWeight)
            continue;

        ImageMaskComponent component;
        component.layerIndex = components.size();
        component.bounds = bounds;

        const QColor autoColor = layerIndex < sourceColors.size() ? sourceColors[layerIndex] : QColor(Qt::white);
        if (component.layerIndex < storedLayers.size()) {
            const ImageColorLayer &stored = storedLayers[component.layerIndex];
            component.color = stored.autoMaskColor ? autoColor : stored.maskColor;
        } else {
            component.color = autoColor;
        }

        component.mask = QImage(bounds.size(), QImage::Format_ARGB32);
        component.mask.fill(Qt::transparent);
        for (int index : pixels) {
            const int px = index % width;
            const int py = index / width;
            QColor maskPixel = component.color;
            maskPixel.setAlpha(image.pixelColor(px, py).alpha());
            component.mask.setPixelColor(px - bounds.left(), py - bounds.top(), maskPixel);
        }
        components.append(component);
    }

    return components;
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
        for (int i = 0; i < components.size(); ++i)
            props.append({QStringLiteral("Слой %1: Цвет").arg(i + 1), components[i].color.name()});
    }

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
