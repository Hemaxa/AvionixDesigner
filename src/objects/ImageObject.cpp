#include "ImageObject.h"

#include "ProportionalResize.h"

#include <QBuffer>
#include <QHash>
#include <QPainter>
#include <QSvgRenderer>

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
}

void ImageObject::setSvgData(const QByteArray &data, const QString &name, const QSize &defaultSize)
{
    m_sourceBytes = data;
    m_sourceImage = QImage();
    sourceName = name;
    format = QStringLiteral("svg");
    width = qMax(1, defaultSize.width());
    height = qMax(1, defaultSize.height());
}

QImage ImageObject::renderedImage() const
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

QColor ImageObject::effectiveMaskColor() const
{
    if (!autoMaskColor && maskColor.isValid())
        return maskColor;

    const QImage image = renderedImage().convertToFormat(QImage::Format_ARGB32);
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
        return;
    }

    m_sourceImage.loadFromData(m_sourceBytes, "PNG");
    if (!m_sourceImage.isNull()) {
        if (width <= 1.0)
            width = m_sourceImage.width();
        if (height <= 1.0)
            height = m_sourceImage.height();
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
    return {
        {"Источник", sourceName},
        {"Тип", format},
        {"X", QString::number(x)},
        {"Y", QString::number(y)},
        {"Ширина", QString::number(width)},
        {"Высота", QString::number(height)},
        {"Автоцвет", autoMaskColor ? QStringLiteral("да") : QStringLiteral("нет")},
        {"Цвет маски", effectiveMaskColor().name()}
    };
}

QRectF ImageObject::getBoundingRect() const
{
    return QRectF(x, y, width, height);
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

    const QRectF oldRect(x, y, qMax(1.0, width), qMax(1.0, height));
    const auto resized = proportionalResizeRect(oldRect, edgeFlags, dx, dy);
    x = resized.rect.left();
    y = resized.rect.top();
    width = qMax(1.0, resized.rect.width());
    height = qMax(1.0, resized.rect.height());
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
    } else if (name == QStringLiteral("Автоцвет")) {
        const QString normalized = value.trimmed().toLower();
        if (normalized == QStringLiteral("да") || normalized == QStringLiteral("yes")
            || normalized == QStringLiteral("true") || normalized == QStringLiteral("1")) {
            autoMaskColor = true;
            ok = true;
        } else if (normalized == QStringLiteral("нет") || normalized == QStringLiteral("no")
                   || normalized == QStringLiteral("false") || normalized == QStringLiteral("0")) {
            autoMaskColor = false;
            ok = true;
        }
    } else if (name == QStringLiteral("Цвет маски")) {
        const QColor color(value);
        ok = color.isValid();
        if (ok) {
            maskColor = color;
            autoMaskColor = false;
        }
    }

    if (ok)
        emit changed();
    return ok;
}
