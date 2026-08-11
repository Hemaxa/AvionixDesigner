#include "RotationObject.h"
#include "BitParser.h"
#include "ProportionalResize.h"
#include "XmlReader.h"
#include <QtMath>

RotationObject::RotationObject(QObject *parent) : BaseObject(parent), color(Qt::white) {}

namespace {
RotationObjectLayer layerFromInit(const QString &hexInit, const ParamSchema &schema)
{
    RotationObjectLayer layer;
    if (schema.contains("enb"))
        layer.enabled = BitParser::extract(hexInit, schema["enb"].offset, schema["enb"].size) != 0;
    if (schema.contains("xrot"))
        layer.xRot = BitParser::extract(hexInit, schema["xrot"].offset, schema["xrot"].size);
    if (schema.contains("yrot"))
        layer.yRot = BitParser::extract(hexInit, schema["yrot"].offset, schema["yrot"].size);
    if (schema.contains("top"))
        layer.top = BitParser::extractSigned(hexInit, schema["top"].offset, schema["top"].size);
    if (schema.contains("left"))
        layer.left = BitParser::extractSigned(hexInit, schema["left"].offset, schema["left"].size);
    if (schema.contains("bottom"))
        layer.bottom = BitParser::extractSigned(hexInit, schema["bottom"].offset, schema["bottom"].size);
    if (schema.contains("right"))
        layer.right = BitParser::extractSigned(hexInit, schema["right"].offset, schema["right"].size);
    if (schema.contains("color"))
        layer.color = BitParser::parseColor(BitParser::extract(hexInit, schema["color"].offset, schema["color"].size));
    if (schema.contains("sin"))
        layer.sinVal = BitParser::extractSigned(hexInit, schema["sin"].offset, schema["sin"].size);
    if (schema.contains("cos"))
        layer.cosVal = BitParser::extractSigned(hexInit, schema["cos"].offset, schema["cos"].size);
    return layer;
}
}

void RotationObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    //парсинг центра вращения (пиксельные координаты)
    if (schema.contains("xrot")) 
        xRot = BitParser::extract(hexInit, schema["xrot"].offset, schema["xrot"].size);
    if (schema.contains("yrot")) 
        yRot = BitParser::extract(hexInit, schema["yrot"].offset, schema["yrot"].size);

    //парсинг смещений от точки вращения (знаковые пиксельные расстояния)
    if (schema.contains("top")) 
        top = BitParser::extractSigned(hexInit, schema["top"].offset, schema["top"].size);
    if (schema.contains("left")) 
        left = BitParser::extractSigned(hexInit, schema["left"].offset, schema["left"].size);
    if (schema.contains("bottom")) 
        bottom = BitParser::extractSigned(hexInit, schema["bottom"].offset, schema["bottom"].size);
    if (schema.contains("right")) 
        right = BitParser::extractSigned(hexInit, schema["right"].offset, schema["right"].size);

    //парсинг цвета
    if (schema.contains("color")) {
        quint32 colorVal = BitParser::extract(hexInit, schema["color"].offset, schema["color"].size);
        color = BitParser::parseColor(colorVal);
    }

    //парсинг синуса и косинуса (знаковые, с фиксированной точкой xx.xxxxxxxxxxxxxxxx)
    if (schema.contains("sin")) {
        sinVal = BitParser::extractSigned(hexInit, schema["sin"].offset, schema["sin"].size);
    }
    if (schema.contains("cos")) {
        cosVal = BitParser::extractSigned(hexInit, schema["cos"].offset, schema["cos"].size);
    }

    m_layers = {currentLayer()};
}

void RotationObject::parseExtraData(const QDomElement &element)
{
    QDomElement dataEl = element.firstChildElement("data");
    if (dataEl.isNull()) return;

    //чтение размеров маски
    int w = XmlReader::readInt(dataEl, "width", 0);
    int h = XmlReader::readInt(dataEl, "height", 0);
    QString text = dataEl.text().trimmed();

    if (w <= 0 || h <= 0) return;

    setMaskFromDataText(text, w, h);
}

void RotationObject::draw(QPainter &painter)
{
    if (maskImage.isNull()) return;

    painter.save();

    const QVector<RotationObjectLayer> layers = m_layers.isEmpty() ? QVector<RotationObjectLayer>{currentLayer()} : m_layers;
    for (const RotationObjectLayer &layer : layers) {
        if (!layer.enabled)
            continue;

        QImage layerImage = maskImage;
        if (!layerImage.isNull()) {
            for (int y = 0; y < layerImage.height(); ++y) {
                for (int x = 0; x < layerImage.width(); ++x) {
                    QColor pixel = layerImage.pixelColor(x, y);
                    if (pixel.alpha() == 0)
                        continue;
                    pixel.setRed(layer.color.red());
                    pixel.setGreen(layer.color.green());
                    pixel.setBlue(layer.color.blue());
                    layerImage.setPixelColor(x, y, pixel);
                }
            }
        }

        const double angleDeg = qRadiansToDegrees(qAtan2(layer.sinVal / 65536.0, layer.cosVal / 65536.0));
        painter.save();
        if (qAbs(angleDeg) > 0.01) {
            painter.translate(layer.xRot, layer.yRot);
            painter.rotate(angleDeg);
            painter.drawImage(QPointF(layer.left, layer.top), layerImage);
        } else {
            painter.drawImage(QPointF(layer.xRot + layer.left, layer.yRot + layer.top), layerImage);
        }
        painter.restore();
    }
    painter.restore();
}

QString RotationObject::getTypeName() const
{
    return "RotationObject";
}

QString RotationObject::getDisplayName() const
{
    return "rotation_object";
}

double RotationObject::getAngleDegrees() const
{
    //sinVal и cosVal — значения с фиксированной точкой xx.xxxxxxxxxxxxxxxx
    //2 бита целая часть, 16 бит дробная → делим на 65536.0 (декодирование из int в float)
    double s = sinVal / 65536.0;
    double c = cosVal / 65536.0;
    return qRadiansToDegrees(qAtan2(s, c));
}

QList<QPair<QString, QString>> RotationObject::getProperties() const
{
    QList<QPair<QString, QString>> props = {
        {"X вращения", QString::number(xRot)},
        {"Y вращения", QString::number(yRot)},
        {"Угол (°)", QString::number(getAngleDegrees(), 'f', 2)},
        {"Top (смещ.)", QString::number(top)},
        {"Left (смещ.)", QString::number(left)},
        {"Bottom (смещ.)", QString::number(bottom)},
        {"Right (смещ.)", QString::number(right)},
        {"Позиция X", QString::number(xRot + left)},
        {"Позиция Y", QString::number(yRot + top)},
        {"Цвет", color.name()}
    };
    
    if (!maskImage.isNull()) {
        props.append({"Маска", QString("%1x%2").arg(maskImage.width()).arg(maskImage.height())});
    }
    if (m_layers.size() > 1)
        props.append({"Цветовых слоев", QString::number(m_layers.size())});
    
    return props;
}

QRectF RotationObject::getBoundingRect() const
{
    const QVector<RotationObjectLayer> layers = m_layers.isEmpty() ? QVector<RotationObjectLayer>{currentLayer()} : m_layers;
    QRectF bounds;
    bool hasBounds = false;
    for (const RotationObjectLayer &layer : layers) {
        const QRectF layerBounds(layer.xRot + layer.left,
                                 layer.yRot + layer.top,
                                 layer.right - layer.left,
                                 layer.bottom - layer.top);
        bounds = hasBounds ? bounds.united(layerBounds) : layerBounds;
        hasBounds = true;
    }
    return hasBounds ? bounds : QRectF();
}

bool RotationObject::contains(const QPointF &point) const
{
    // Учитываем поворот для определения попадания мыши
    QTransform transform;
    transform.translate(xRot, yRot);
    transform.rotate(getAngleDegrees());
    
    // Инвертируем трансформацию, чтобы перевести точку клика в локальные координаты
    QTransform invTransform = transform.inverted();
    QPointF localPoint = invTransform.map(point);
    
    QRectF localRect(left, top, right - left, bottom - top);
    return localRect.contains(localPoint);
}

void RotationObject::moveBy(double dx, double dy)
{
    xRot += dx;
    yRot += dy;
    for (RotationObjectLayer &layer : m_layers) {
        layer.xRot += dx;
        layer.yRot += dy;
    }
    emit changed();
}

void RotationObject::resizeBy(int edgeFlags, double dx, double dy)
{
    if (!canResize())
        return;

    // Так как размеры хранятся как смещения (top, left, bottom, right),
    // нам нужно учитывать текущий поворот.
    // Проще всего конвертировать dx, dy в локальные координаты.
    double angleRad = qDegreesToRadians(-getAngleDegrees()); // Обратный поворот
    double localDx = dx * qCos(angleRad) - dy * qSin(angleRad);
    double localDy = dx * qSin(angleRad) + dy * qCos(angleRad);

    const QRectF oldRect(left, top, qMax(1.0, right - left), qMax(1.0, bottom - top));
    const auto resized = proportionalResizeRect(oldRect, edgeFlags, localDx, localDy);
    left = resized.rect.left();
    top = resized.rect.top();
    right = resized.rect.right();
    bottom = resized.rect.bottom();

    const int newWidth = qMax(1, qRound(resized.rect.width()));
    const int newHeight = qMax(1, qRound(resized.rect.height()));
    if (!maskImage.isNull() && (maskImage.width() != newWidth || maskImage.height() != newHeight)) {
        maskImage = maskImage.scaled(newWidth, newHeight, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    syncFirstLayerFromPublicFields();
    
    emit changed();
}

void RotationObject::setRotation(double angle)
{
    double angleRad = qDegreesToRadians(angle);
    sinVal = qRound(qSin(angleRad) * 65536.0);
    cosVal = qRound(qCos(angleRad) * 65536.0);
    for (RotationObjectLayer &layer : m_layers) {
        layer.sinVal = sinVal;
        layer.cosVal = cosVal;
    }
    emit changed();
}

bool RotationObject::setObjectProperty(const QString &name, const QString &value)
{
    bool ok = false;
    
    if (name == "Left" || name == "Left (смещ.)") {
        if (!canResize()) {
            setValidationMessage(QObject::tr("Размер растрового объекта заблокирован в ограниченном режиме."));
            return false;
        }
        left = value.toDouble(&ok);
    }
    else if (name == "Top" || name == "Top (смещ.)") {
        if (!canResize()) {
            setValidationMessage(QObject::tr("Размер растрового объекта заблокирован в ограниченном режиме."));
            return false;
        }
        top = value.toDouble(&ok);
    }
    else if (name == "Right" || name == "Right (смещ.)") {
        if (!canResize()) {
            setValidationMessage(QObject::tr("Размер растрового объекта заблокирован в ограниченном режиме."));
            return false;
        }
        right = value.toDouble(&ok);
    }
    else if (name == "Bottom" || name == "Bottom (смещ.)") {
        if (!canResize()) {
            setValidationMessage(QObject::tr("Размер растрового объекта заблокирован в ограниченном режиме."));
            return false;
        }
        bottom = value.toDouble(&ok);
    }
    else if (name == "X вращения") {
        xRot = value.toDouble(&ok);
    }
    else if (name == "Y вращения") {
        yRot = value.toDouble(&ok);
    }
    else if (name == "Угол (°)") {
        double angleDeg = value.toDouble(&ok);
        if (ok) {
            double angleRad = qDegreesToRadians(angleDeg);
            sinVal = qRound(qSin(angleRad) * 65536.0);
            cosVal = qRound(qCos(angleRad) * 65536.0);
        }
    }
    else if (name == "Цвет") {
        color = QColor(value);
        ok = color.isValid();
    }
    
    if (ok) {
        syncFirstLayerFromPublicFields();
        emit changed();
    }
    return ok;
}

QMap<QString, quint32> RotationObject::serializeParams() const
{
    const int width = qMax(1, qRound(right - left));
    return {
        {"enb", isViewVisible() ? 1u : 0u},
        {"xrot", static_cast<quint32>(xRot)},
        {"yrot", static_cast<quint32>(yRot)},
        {"top", static_cast<quint32>(static_cast<qint32>(top))},
        {"left", static_cast<quint32>(static_cast<qint32>(left))},
        {"bottom", static_cast<quint32>(static_cast<qint32>(bottom))},
        {"right", static_cast<quint32>(static_cast<qint32>(right))},
        {"sq", static_cast<quint32>(qCeil(width / 8.0))},
        {"color", BitParser::colorToBgr(color)},
        {"sin", static_cast<quint32>(sinVal)},
        {"cos", static_cast<quint32>(cosVal)}
    };
}

QList<QMap<QString, quint32>> RotationObject::serializeLayerParams() const
{
    if (m_layers.isEmpty())
        return {serializeParams()};

    QList<QMap<QString, quint32>> params;
    params.reserve(m_layers.size());
    for (const RotationObjectLayer &layer : m_layers) {
        const int width = qMax(1, qRound(layer.right - layer.left));
        params.append({
            {"enb", layer.enabled && isViewVisible() ? 1u : 0u},
            {"xrot", static_cast<quint32>(layer.xRot)},
            {"yrot", static_cast<quint32>(layer.yRot)},
            {"top", static_cast<quint32>(static_cast<qint32>(layer.top))},
            {"left", static_cast<quint32>(static_cast<qint32>(layer.left))},
            {"bottom", static_cast<quint32>(static_cast<qint32>(layer.bottom))},
            {"right", static_cast<quint32>(static_cast<qint32>(layer.right))},
            {"sq", static_cast<quint32>(qCeil(width / 8.0))},
            {"color", BitParser::colorToBgr(layer.color)},
            {"sin", static_cast<quint32>(layer.sinVal)},
            {"cos", static_cast<quint32>(layer.cosVal)}
        });
    }
    return params;
}

void RotationObject::setHardwareLayersFromInitHex(const QStringList &initHexList, const ParamSchema &schema)
{
    QVector<RotationObjectLayer> layers;
    for (const QString &hex : initHexList) {
        const QString trimmed = hex.trimmed();
        if (!trimmed.isEmpty())
            layers.append(layerFromInit(trimmed, schema));
    }
    if (layers.isEmpty())
        layers.append(currentLayer());

    m_layers = layers;
    applyLayerToPublicFields(m_layers.first());
}

QVector<RotationObjectLayer> RotationObject::hardwareLayers() const
{
    return m_layers;
}

RotationObjectLayer RotationObject::currentLayer() const
{
    RotationObjectLayer layer;
    layer.left = left;
    layer.top = top;
    layer.right = right;
    layer.bottom = bottom;
    layer.xRot = xRot;
    layer.yRot = yRot;
    layer.sinVal = sinVal;
    layer.cosVal = cosVal;
    layer.color = color;
    layer.enabled = isViewVisible();
    return layer;
}

void RotationObject::applyLayerToPublicFields(const RotationObjectLayer &layer)
{
    left = layer.left;
    top = layer.top;
    right = layer.right;
    bottom = layer.bottom;
    xRot = layer.xRot;
    yRot = layer.yRot;
    sinVal = layer.sinVal;
    cosVal = layer.cosVal;
    color = layer.color;
}

void RotationObject::syncFirstLayerFromPublicFields()
{
    if (m_layers.isEmpty())
        m_layers.append(currentLayer());
    else
        m_layers[0] = currentLayer();
}

void RotationObject::setMaskFromDataText(const QString &text, int width, int height)
{
    maskImage = QImage(width, height, QImage::Format_ARGB32);
    maskImage.fill(Qt::transparent);

    QVector<int> values;
    values.reserve(width * height);
    for (const QChar ch : text) {
        if (!ch.isDigit())
            continue;
        values.append(qBound(0, ch.digitValue(), 7));
    }

    int idx = 0;
    for (int py = 0; py < height; ++py) {
        for (int px = 0; px < width; ++px) {
            const int val = idx < values.size() ? values[idx] : 0;
            ++idx;
            if (val <= 0)
                continue;

            QColor pixelColor = Qt::white;
            pixelColor.setAlpha((val * 255) / 7);
            maskImage.setPixelColor(px, py, pixelColor);
        }
    }
}
