#include "RibbonScaleObject.h"

#include "BitParser.h"

RibbonScaleObject::RibbonScaleObject(QObject *parent) : BaseObject(parent) {}

void RibbonScaleObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    if (schema.contains("enable"))
        enabled = BitParser::extract(hexInit, schema["enable"].offset, schema["enable"].size) != 0;
    if (schema.contains("color"))
        color = BitParser::parseColor(BitParser::extract(hexInit, schema["color"].offset, schema["color"].size));
    if (schema.contains("left"))
        left = BitParser::extract(hexInit, schema["left"].offset, schema["left"].size);
    if (schema.contains("right"))
        right = BitParser::extract(hexInit, schema["right"].offset, schema["right"].size);
    if (schema.contains("top"))
        top = BitParser::extract(hexInit, schema["top"].offset, schema["top"].size);
    if (schema.contains("bottom"))
        bottom = BitParser::extract(hexInit, schema["bottom"].offset, schema["bottom"].size);
    if (schema.contains("width"))
        lineWidth = qMax(1, static_cast<int>(BitParser::extract(hexInit, schema["width"].offset, schema["width"].size)));
    if (schema.contains("period"))
        period = qMax(1, static_cast<int>(BitParser::extract(hexInit, schema["period"].offset, schema["period"].size)));
    if (schema.contains("ystart"))
        yStart = BitParser::extract(hexInit, schema["ystart"].offset, schema["ystart"].size);
}

void RibbonScaleObject::draw(QPainter &painter)
{
    if (!enabled)
        return;

    const QRectF bounds = getBoundingRect();
    if (bounds.isEmpty())
        return;

    painter.save();
    painter.setClipRect(bounds);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    const double step = qMax(1, period);
    double firstY = yStart;
    while (firstY > top) {
        firstY -= step;
    }
    while (firstY + step <= top) {
        firstY += step;
    }

    for (double y = firstY; y <= bottom + lineWidth; y += step) {
        painter.drawRect(QRectF(left, y - lineWidth / 2.0, qMax(1.0, right - left), lineWidth));
    }

    painter.restore();
}

QString RibbonScaleObject::getTypeName() const
{
    return "RibbonScale";
}

QString RibbonScaleObject::getDisplayName() const
{
    return "Ленточная шкала";
}

QList<QPair<QString, QString>> RibbonScaleObject::getProperties() const
{
    return {
        {"Включен", enabled ? "да" : "нет"},
        {"Left", QString::number(left, 'f', 1)},
        {"Right", QString::number(right, 'f', 1)},
        {"Top", QString::number(top, 'f', 1)},
        {"Bottom", QString::number(bottom, 'f', 1)},
        {"Цвет", color.name()},
        {"Ширина риски", QString::number(lineWidth)},
        {"Период", QString::number(period)},
        {"Y start", QString::number(yStart, 'f', 1)}
    };
}

QRectF RibbonScaleObject::getBoundingRect() const
{
    return QRectF(left, top, qMax(1.0, right - left), qMax(1.0, bottom - top));
}

void RibbonScaleObject::moveBy(double dx, double dy)
{
    left += dx;
    right += dx;
    top += dy;
    bottom += dy;
    yStart += dy;
    emit changed();
}

void RibbonScaleObject::resizeBy(int edgeFlags, double dx, double dy)
{
    const QRectF oldRect = getBoundingRect();
    const double oldHeight = qMax(1.0, oldRect.height());
    const double yRelative = (yStart - oldRect.top()) / oldHeight;

    if (edgeFlags & 1) {
        left += dx;
    }
    if (edgeFlags & 2) {
        right += dx;
    }
    if (edgeFlags & 4) {
        top += dy;
    }
    if (edgeFlags & 8) {
        bottom += dy;
    }

    if (right <= left + 1.0)
        right = left + 1.0;
    if (bottom <= top + 1.0)
        bottom = top + 1.0;

    yStart = top + yRelative * qMax(1.0, bottom - top);
    emit changed();
}

bool RibbonScaleObject::setObjectProperty(const QString &name, const QString &value)
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
    } else if (name == "Left") {
        left = value.toDouble(&ok);
    } else if (name == "Right") {
        right = value.toDouble(&ok);
        if (ok)
            right = qMax(left + 1.0, right);
    } else if (name == "Top") {
        top = value.toDouble(&ok);
    } else if (name == "Bottom") {
        bottom = value.toDouble(&ok);
        if (ok)
            bottom = qMax(top + 1.0, bottom);
    } else if (name == "Цвет") {
        color = QColor(value);
        ok = color.isValid();
    } else if (name == "Ширина риски") {
        lineWidth = qMax(1, value.toInt(&ok));
    } else if (name == "Период") {
        period = qMax(1, value.toInt(&ok));
    } else if (name == "Y start") {
        yStart = value.toDouble(&ok);
    }

    if (ok)
        emit changed();
    return ok;
}

QMap<QString, quint32> RibbonScaleObject::serializeParams() const
{
    return {
        {"enable", enabled ? 1u : 0u},
        {"color", BitParser::colorToBgr(color)},
        {"left", static_cast<quint32>(qMax(0, qRound(left)))},
        {"right", static_cast<quint32>(qMax(0, qRound(right)))},
        {"top", static_cast<quint32>(qMax(0, qRound(top)))},
        {"bottom", static_cast<quint32>(qMax(0, qRound(bottom)))},
        {"width", static_cast<quint32>(qBound(1, lineWidth, 15))},
        {"period", static_cast<quint32>(qBound(1, period, 255))},
        {"ystart", static_cast<quint32>(qMax(0, qRound(yStart)))}
    };
}
