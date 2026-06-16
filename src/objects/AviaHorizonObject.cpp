#include "AviaHorizonObject.h"

#include "BitParser.h"
#include "ProjectManager.h"

#include <QtMath>

AviaHorizonObject::AviaHorizonObject(QObject *parent) : BaseObject(parent) {}

void AviaHorizonObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    if (schema.contains("enb"))
        enabled = BitParser::extract(hexInit, schema["enb"].offset, schema["enb"].size) != 0;
    if (schema.contains("earth"))
        earthColor = BitParser::parseColor(BitParser::extract(hexInit, schema["earth"].offset, schema["earth"].size));
    if (schema.contains("sky"))
        skyColor = BitParser::parseColor(BitParser::extract(hexInit, schema["sky"].offset, schema["sky"].size));
    if (schema.contains("hline"))
        horizonLineColor = BitParser::parseColor(BitParser::extract(hexInit, schema["hline"].offset, schema["hline"].size));
    if (schema.contains("width"))
        lineWidth = BitParser::extract(hexInit, schema["width"].offset, schema["width"].size);
    if (schema.contains("xo"))
        xCenter = BitParser::extract(hexInit, schema["xo"].offset, schema["xo"].size);
    if (schema.contains("yo"))
        yCenter = BitParser::extract(hexInit, schema["yo"].offset, schema["yo"].size);
    if (schema.contains("sn"))
        sinVal = BitParser::extractSigned(hexInit, schema["sn"].offset, schema["sn"].size);
    if (schema.contains("cs"))
        cosVal = BitParser::extractSigned(hexInit, schema["cs"].offset, schema["cs"].size);
}

void AviaHorizonObject::draw(QPainter &painter)
{
    if (!enabled)
        return;

    const QRectF localRect = getLocalAreaRect();
    const double span = qSqrt(areaWidth * areaWidth + areaHeight * areaHeight) * 1.5;
    const double band = qMax(1.0, lineWidth);

    painter.save();
    painter.translate(xCenter, yCenter);
    painter.rotate(getAngleDegrees());
    painter.setClipRect(localRect);
    painter.setPen(Qt::NoPen);

    painter.setBrush(skyColor);
    painter.drawRect(QRectF(-span, -span, span * 2.0, span));

    painter.setBrush(earthColor);
    painter.drawRect(QRectF(-span, 0.0, span * 2.0, span));

    painter.setBrush(horizonLineColor);
    painter.drawRect(QRectF(-span, -band / 2.0, span * 2.0, band));
    painter.restore();
}

QString AviaHorizonObject::getTypeName() const
{
    return "AviaHorizon";
}

QString AviaHorizonObject::getDisplayName() const
{
    return "Авиагоризонт";
}

QList<QPair<QString, QString>> AviaHorizonObject::getProperties() const
{
    return {
        {"Включен", enabled ? "да" : "нет"},
        {"Центр X", QString::number(xCenter, 'f', 1)},
        {"Центр Y", QString::number(yCenter, 'f', 1)},
        {"Ширина области", QString::number(areaWidth, 'f', 1)},
        {"Высота области", QString::number(areaHeight, 'f', 1)},
        {"Угол (°)", QString::number(getAngleDegrees(), 'f', 2)},
        {"Ширина линии", QString::number(lineWidth, 'f', 1)},
        {"Цвет неба", skyColor.name()},
        {"Цвет земли", earthColor.name()},
        {"Цвет линии", horizonLineColor.name()}
    };
}

QRectF AviaHorizonObject::getBoundingRect() const
{
    QTransform transform;
    transform.translate(xCenter, yCenter);
    transform.rotate(getAngleDegrees());
    return transform.mapRect(getLocalAreaRect());
}

void AviaHorizonObject::moveBy(double dx, double dy)
{
    xCenter += dx;
    yCenter += dy;
    emit changed();
}

void AviaHorizonObject::resizeBy(int edgeFlags, double dx, double dy)
{
    const double angleRad = qDegreesToRadians(-getAngleDegrees());
    const double localDx = dx * qCos(angleRad) - dy * qSin(angleRad);
    const double localDy = dx * qSin(angleRad) + dy * qCos(angleRad);

    if (edgeFlags & 1) {
        areaWidth = qMax(48.0, areaWidth - localDx);
        xCenter += (localDx * qCos(qDegreesToRadians(getAngleDegrees()))) / 2.0;
        yCenter += (localDx * qSin(qDegreesToRadians(getAngleDegrees()))) / 2.0;
    }
    if (edgeFlags & 2) {
        areaWidth = qMax(48.0, areaWidth + localDx);
        xCenter += (localDx * qCos(qDegreesToRadians(getAngleDegrees()))) / 2.0;
        yCenter += (localDx * qSin(qDegreesToRadians(getAngleDegrees()))) / 2.0;
    }
    if (edgeFlags & 4) {
        areaHeight = qMax(48.0, areaHeight - localDy);
        xCenter += (-localDy * qSin(qDegreesToRadians(getAngleDegrees()))) / 2.0;
        yCenter += (localDy * qCos(qDegreesToRadians(getAngleDegrees()))) / 2.0;
    }
    if (edgeFlags & 8) {
        areaHeight = qMax(48.0, areaHeight + localDy);
        xCenter += (-localDy * qSin(qDegreesToRadians(getAngleDegrees()))) / 2.0;
        yCenter += (localDy * qCos(qDegreesToRadians(getAngleDegrees()))) / 2.0;
    }

    emit changed();
}

void AviaHorizonObject::setRotation(double angle)
{
    const double angleRad = qDegreesToRadians(angle);
    sinVal = qRound(qSin(angleRad) * 65536.0);
    cosVal = qRound(qCos(angleRad) * 65536.0);
    emit changed();
}

bool AviaHorizonObject::setObjectProperty(const QString &name, const QString &value)
{
    bool ok = false;

    if (name == "Включен") {
        const QString normalized = value.trimmed().toLower();
        if (normalized == "да" || normalized == "yes" || normalized == "1" || normalized == "true") {
            enabled = true;
            ok = true;
        } else if (normalized == "нет" || normalized == "no" || normalized == "0" || normalized == "false") {
            enabled = false;
            ok = true;
        }
    }
    else if (name == "Центр X") {
        xCenter = value.toDouble(&ok);
    }
    else if (name == "Центр Y") {
        yCenter = value.toDouble(&ok);
    }
    else if (name == "Ширина области") {
        areaWidth = value.toDouble(&ok);
        if (ok)
            areaWidth = qMax(48.0, areaWidth);
    }
    else if (name == "Высота области") {
        areaHeight = value.toDouble(&ok);
        if (ok)
            areaHeight = qMax(48.0, areaHeight);
    }
    else if (name == "Угол (°)") {
        const double angleDeg = value.toDouble(&ok);
        if (ok) {
            setRotation(angleDeg);
            return true;
        }
    }
    else if (name == "Ширина линии") {
        lineWidth = value.toDouble(&ok);
        if (ok)
            lineWidth = qMax(1.0, lineWidth);
    }
    else if (name == "Цвет неба") {
        skyColor = QColor(value);
        ok = skyColor.isValid();
    }
    else if (name == "Цвет земли") {
        earthColor = QColor(value);
        ok = earthColor.isValid();
    }
    else if (name == "Цвет линии") {
        horizonLineColor = QColor(value);
        ok = horizonLineColor.isValid();
    }

    if (ok)
        emit changed();
    return ok;
}

QMap<QString, quint32> AviaHorizonObject::serializeParams() const
{
    return {
        {"enb", enabled ? 1u : 0u},
        {"earth", BitParser::colorToBgr(earthColor)},
        {"sky", BitParser::colorToBgr(skyColor)},
        {"hline", BitParser::colorToBgr(horizonLineColor)},
        {"width", static_cast<quint32>(qBound(0, qRound(lineWidth), 15))},
        {"xo", static_cast<quint32>(qMax(0, qRound(xCenter)))},
        {"yo", static_cast<quint32>(qMax(0, qRound(yCenter)))},
        {"sn", static_cast<quint32>(static_cast<qint32>(sinVal))},
        {"cs", static_cast<quint32>(static_cast<qint32>(cosVal))}
    };
}

double AviaHorizonObject::getAngleDegrees() const
{
    const double s = sinVal / 65536.0;
    const double c = cosVal / 65536.0;
    return qRadiansToDegrees(qAtan2(s, c));
}

bool AviaHorizonObject::contains(const QPointF &point) const
{
    QTransform transform;
    transform.translate(xCenter, yCenter);
    transform.rotate(getAngleDegrees());
    const QPointF localPoint = transform.inverted().map(point);
    return getLocalAreaRect().contains(localPoint);
}

QRectF AviaHorizonObject::getLocalAreaRect() const
{
    return QRectF(-areaWidth / 2.0, -areaHeight / 2.0, areaWidth, areaHeight);
}
