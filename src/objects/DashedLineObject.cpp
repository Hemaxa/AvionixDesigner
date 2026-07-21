#include "DashedLineObject.h"

#include "BitParser.h"
#include "ProportionalResize.h"

#include <QPainterPath>
#include <QPainterPathStroker>
#include <QtMath>

namespace {
double distance(const QPointF &a, const QPointF &b)
{
    return qSqrt(qPow(b.x() - a.x(), 2.0) + qPow(b.y() - a.y(), 2.0));
}
}

DashedLineObject::DashedLineObject(QObject *parent) : BaseObject(parent) {}

void DashedLineObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    if (schema.contains("Enable"))
        enabled = BitParser::extract(hexInit, schema["Enable"].offset, schema["Enable"].size) != 0;
    else if (schema.contains("enable"))
        enabled = BitParser::extract(hexInit, schema["enable"].offset, schema["enable"].size) != 0;

    if (schema.contains("Color"))
        color = BitParser::parseColor(BitParser::extract(hexInit, schema["Color"].offset, schema["Color"].size));
    else if (schema.contains("color"))
        color = BitParser::parseColor(BitParser::extract(hexInit, schema["color"].offset, schema["color"].size));

    if (schema.contains("xo"))
        x0 = BitParser::extract(hexInit, schema["xo"].offset, schema["xo"].size);
    if (schema.contains("yo"))
        y0 = BitParser::extract(hexInit, schema["yo"].offset, schema["yo"].size);

    const int dt = schema.contains("dt") ? BitParser::extractSigned(hexInit, schema["dt"].offset, schema["dt"].size) : 0;
    const int aRaw = schema.contains("a") ? BitParser::extractSigned(hexInit, schema["a"].offset, schema["a"].size) : 256;
    const int bRaw = schema.contains("b") ? BitParser::extractSigned(hexInit, schema["b"].offset, schema["b"].size) : 0;

    x1 = x0 + (aRaw / 256.0) * dt;
    y1 = y0 + (bRaw / 256.0) * dt;

    const int stepPowMinus1 = schema.contains("StepPow") ? BitParser::extract(hexInit, schema["StepPow"].offset, schema["StepPow"].size)
                                                         : (schema.contains("StepPow-1") ? BitParser::extract(hexInit, schema["StepPow-1"].offset, schema["StepPow-1"].size) : 3);
    dashPeriod = 1 << (qBound(0, stepPowMinus1, 7) + 1);
    dashLength = schema.contains("Length") ? BitParser::extract(hexInit, schema["Length"].offset, schema["Length"].size) + 1
                                           : (schema.contains("Length-1") ? BitParser::extract(hexInit, schema["Length-1"].offset, schema["Length-1"].size) + 1 : 8);
    dashPhase = schema.contains("Phase") ? BitParser::extract(hexInit, schema["Phase"].offset, schema["Phase"].size) : 0;
    lineWidth = schema.contains("Width") ? BitParser::extract(hexInit, schema["Width"].offset, schema["Width"].size) + 1
                                         : (schema.contains("Width-1") ? BitParser::extract(hexInit, schema["Width-1"].offset, schema["Width-1"].size) + 1 : 2);
}

void DashedLineObject::draw(QPainter &painter)
{
    if (!enabled)
        return;

    const QPointF start(x0, y0);
    const QPointF end(x1, y1);
    const double totalLength = distance(start, end);
    if (totalLength < 0.5)
        return;

    const QPointF direction((end.x() - start.x()) / totalLength, (end.y() - start.y()) / totalLength);
    const int period = qMax(2, dashPeriod);
    const int length = qBound(1, dashLength, period);
    const int phase = ((dashPhase % period) + period) % period;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, lineWidth, Qt::SolidLine, Qt::FlatCap));

    for (double segmentStart = -phase; segmentStart < totalLength; segmentStart += period) {
        const double visibleStart = qMax(0.0, segmentStart);
        const double visibleEnd = qMin(totalLength, segmentStart + length);
        if (visibleEnd <= visibleStart)
            continue;

        const QPointF p0 = start + direction * visibleStart;
        const QPointF p1 = start + direction * visibleEnd;
        painter.drawLine(p0, p1);
    }

    painter.restore();
}

QString DashedLineObject::getTypeName() const
{
    return "DashedLine";
}

QString DashedLineObject::getDisplayName() const
{
    return "Штриховая линия";
}

QList<QPair<QString, QString>> DashedLineObject::getProperties() const
{
    return {
        {"Включен", enabled ? "да" : "нет"},
        {"X0", QString::number(x0, 'f', 1)},
        {"Y0", QString::number(y0, 'f', 1)},
        {"X1", QString::number(x1, 'f', 1)},
        {"Y1", QString::number(y1, 'f', 1)},
        {"Цвет", color.name()},
        {"Период штриха", QString::number(dashPeriod)},
        {"Длина штриха", QString::number(dashLength)},
        {"Фаза", QString::number(dashPhase)},
        {"Ширина", QString::number(lineWidth)}
    };
}

QRectF DashedLineObject::getBoundingRect() const
{
    const QRectF lineRect(QPointF(qMin(x0, x1), qMin(y0, y1)), QPointF(qMax(x0, x1), qMax(y0, y1)));
    const double margin = qMax(2.0, lineWidth / 2.0 + 2.0);
    return lineRect.adjusted(-margin, -margin, margin, margin);
}

bool DashedLineObject::contains(const QPointF &point) const
{
    QPainterPath path;
    path.moveTo(x0, y0);
    path.lineTo(x1, y1);
    QPainterPathStroker stroker;
    stroker.setWidth(qMax(6.0, static_cast<double>(lineWidth) + 6.0));
    return stroker.createStroke(path).contains(point);
}

void DashedLineObject::moveBy(double dx, double dy)
{
    x0 += dx;
    y0 += dy;
    x1 += dx;
    y1 += dy;
    emit changed();
}

void DashedLineObject::resizeBy(int edgeFlags, double dx, double dy)
{
    const QRectF oldBounds = getBoundingRect();
    const auto resized = proportionalResizeRect(oldBounds, edgeFlags, dx, dy);

    const double oldWidth = qMax(1.0, oldBounds.width());
    const double oldHeight = qMax(1.0, oldBounds.height());

    auto mapPoint = [&](const QPointF &p) {
        const double relX = (p.x() - oldBounds.left()) / oldWidth;
        const double relY = (p.y() - oldBounds.top()) / oldHeight;
        return QPointF(
            resized.rect.left() + relX * resized.rect.width(),
            resized.rect.top() + relY * resized.rect.height()
        );
    };

    const QPointF newStart = mapPoint(QPointF(x0, y0));
    const QPointF newEnd = mapPoint(QPointF(x1, y1));
    x0 = newStart.x();
    y0 = newStart.y();
    x1 = newEnd.x();
    y1 = newEnd.y();
    lineWidth = qMax(1, qRound(lineWidth * resized.scale));
    emit changed();
}

bool DashedLineObject::setObjectProperty(const QString &name, const QString &value)
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
    } else if (name == "X0") {
        x0 = value.toDouble(&ok);
    } else if (name == "Y0") {
        y0 = value.toDouble(&ok);
    } else if (name == "X1") {
        x1 = value.toDouble(&ok);
    } else if (name == "Y1") {
        y1 = value.toDouble(&ok);
    } else if (name == "Цвет") {
        color = QColor(value);
        ok = color.isValid();
    } else if (name == "Период штриха") {
        dashPeriod = normalizedDashPeriod(value.toInt(&ok));
    } else if (name == "Длина штриха") {
        dashLength = qMax(1, value.toInt(&ok));
        if (ok)
            dashLength = qMin(dashLength, normalizedDashPeriod(dashPeriod));
    } else if (name == "Фаза") {
        dashPhase = qMax(0, value.toInt(&ok));
    } else if (name == "Ширина") {
        lineWidth = qMax(1, value.toInt(&ok));
    }

    if (ok)
        emit changed();
    return ok;
}

QMap<QString, quint32> DashedLineObject::serializeParams() const
{
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const int dt = qMax(1, qRound(qSqrt(dx * dx + dy * dy)));
    const int period = normalizedDashPeriod(dashPeriod);

    int exponent = 1;
    int value = period;
    while (value > 2 && exponent < 8) {
        value >>= 1;
        ++exponent;
    }

    return {
        {"Enable", enabled ? 1u : 0u},
        {"Color", BitParser::colorToBgr(color)},
        {"xo", static_cast<quint32>(qMax(0, qRound(x0)))},
        {"yo", static_cast<quint32>(qMax(0, qRound(y0)))},
        {"dt", static_cast<quint32>(static_cast<qint32>(dt))},
        {"a", static_cast<quint32>(static_cast<qint32>(qRound((dx / dt) * 256.0)))},
        {"b", static_cast<quint32>(static_cast<qint32>(qRound((dy / dt) * 256.0)))},
        {"StepPow", static_cast<quint32>(qBound(0, exponent - 1, 7))},
        {"Length", static_cast<quint32>(qBound(0, dashLength - 1, 255))},
        {"Phase", static_cast<quint32>(qBound(0, dashPhase, 255))},
        {"Width", static_cast<quint32>(qBound(0, lineWidth - 1, 15))}
    };
}

int DashedLineObject::normalizedDashPeriod(int candidate) const
{
    int period = qMax(2, candidate);
    int power = 2;
    while (power < period && power < 256) {
        power <<= 1;
    }

    if (power == period || power == 2)
        return power;

    const int lower = power >> 1;
    return (period - lower) <= (power - period) ? lower : power;
}
