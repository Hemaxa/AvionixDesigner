//RectangleObject - класс векторного прямоугольного объекта с заливкой и обводкой

#include "RectangleObject.h"
#include "BitParser.h"
#include "ProportionalResize.h"
#include "RectanglePainting.h"

#include <QtMath>

RectangleObject::RectangleObject(QObject *parent) : BaseObject(parent), fillColor(Qt::white), strokeColor(Qt::transparent) {}

void RectangleObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    enabled = BitParser::extract(hexInit, schema.value("enb").offset, schema.value("enb").size) != 0;
    //парсинг координат
    //если в XML сказано, что у прямоугольника есть параметр "x0"
    if (schema.contains("x0")) 
        // BitParser вырезает биты по инструкции
        x = BitParser::extract(hexInit, schema["x0"].offset, schema["x0"].size);
    if (schema.contains("y0")) 
        y = BitParser::extract(hexInit, schema["y0"].offset, schema["y0"].size);
    if (schema.contains("w"))  
        width = BitParser::extract(hexInit, schema["w"].offset, schema["w"].size);
    if (schema.contains("h"))  
        height = BitParser::extract(hexInit, schema["h"].offset, schema["h"].size);

    //парсинг цвета заливки
    if (schema.contains("color")) {
        quint32 colorVal = BitParser::extract(hexInit, schema["color"].offset, schema["color"].size);
        fillColor = BitParser::parseColor(colorVal);
    }

    //парсинг цвета обводки
    if (schema.contains("colorb")) {
        quint32 colorVal = BitParser::extract(hexInit, schema["colorb"].offset, schema["colorb"].size);
        strokeColor = BitParser::parseColor(colorVal);
    }
    else {
        strokeColor = Qt::transparent;
    }

    //парсинг толщины обводки
    if (schema.contains("a")) {
        strokeWidth = BitParser::extract(hexInit, schema["a"].offset, schema["a"].size);
    }
    
    //парсинг прозрачности
    if (schema.contains("alph")) {
        int rawAlpha = BitParser::extract(hexInit, schema["alph"].offset, schema["alph"].size);
        alpha = qBound(0, qRound((rawAlpha + 1) * 255.0 / 8.0), 255);
        // Полная прозрачность передаётся выключенным rectangle_a с alph=0.
        if (!enabled && rawAlpha == 0) {
            alpha = 0;
            enabled = true;
        }
        fillColor.setAlpha(alpha);
        explicitAlpha = true;
    } else {
        alpha = 255;
        fillColor.setAlpha(alpha);
        explicitAlpha = false;
    }
}

void RectangleObject::draw(QPainter &painter)
{
    if (!enabled || alpha == 0) return;
    QColor border = strokeColor;
    border.setAlpha(qRound(strokeColor.alpha() * alpha / 255.0));
    paintHardwareRectangle(painter, QRectF(x, y, width, height), fillColor, border, strokeWidth);
}

QString RectangleObject::getTypeName() const
{
    return "Rectangle";
}

QString RectangleObject::getDisplayName() const
{
    return "Прямоугольник";
}

QList<QPair<QString, QString>> RectangleObject::getProperties() const
{
    return {
        {"X", QString::number(x, 'f', 1)},
        {"Y", QString::number(y, 'f', 1)},
        {"Ширина", QString::number(width, 'f', 1)},
        {"Высота", QString::number(height, 'f', 1)},
        {"Заливка", fillColor.name()},
        {"Обводка", strokeColor.name()},
        {"Толщина", QString::number(strokeWidth, 'f', 1)},
        {"Прозрачность", QString::number(qRound((255 - alpha) * 100.0 / 255.0))}
    };
}

QRectF RectangleObject::getBoundingRect() const
{
    return QRectF(x, y, width, height);
}

void RectangleObject::moveBy(double dx, double dy)
{
    x += dx;
    y += dy;
    emit changed();
}

void RectangleObject::resizeBy(int edgeFlags, double dx, double dy, bool proportional)
{
    if (proportional) {
        const auto result = proportionalResizeRect(getBoundingRect(), edgeFlags, dx, dy).rect;
        x = result.x(); y = result.y(); width = result.width(); height = result.height();
        emit changed();
        return;
    }
    // Флаги сторон: 1 — слева, 2 — справа, 4 — сверху, 8 — снизу.
    if (edgeFlags & 1) { // Слева
        x += dx;
        width -= dx;
    }
    if (edgeFlags & 2) { // Справа
        width += dx;
    }
    if (edgeFlags & 4) { // Сверху
        y += dy;
        height -= dy;
    }
    if (edgeFlags & 8) { // Снизу
        height += dy;
    }
    
    if (width < 1.0) width = 1.0;
    if (height < 1.0) height = 1.0;
    
    emit changed();
}

bool RectangleObject::setObjectProperty(const QString &name, const QString &value)
{
    bool ok = false;
    
    if (name == "X") {
        x = value.toDouble(&ok);
    }
    else if (name == "Y") {
        y = value.toDouble(&ok);
    }
    else if (name == "Ширина") {
        width = value.toDouble(&ok);
    }
    else if (name == "Высота") {
        height = value.toDouble(&ok);
    }
    else if (name == "Заливка") {
        fillColor = QColor(value);
        ok = fillColor.isValid();
        if (ok)
            fillColor.setAlpha(alpha);
    }
    else if (name == "Обводка") {
        strokeColor = QColor(value);
        ok = strokeColor.isValid();
    }
    else if (name == "Толщина") {
        strokeWidth = value.toDouble(&ok);
    }
    else if (name == "Прозрачность") {
        const double percent = value.toDouble(&ok);
        if (!ok || !qIsFinite(percent) || percent < 0.0 || percent > 100.0)
            return false;
        alpha = qRound((100.0 - percent) * 255.0 / 100.0);
        if (ok) {
            fillColor.setAlpha(alpha);
            explicitAlpha = true;
        }
    }
    
    if (ok) emit changed();
    return ok;
}

QMap<QString, quint32> RectangleObject::serializeParams() const
{
    return {
        {"enb", enabled && alpha > 0 ? 1u : 0u},
        {"x0", static_cast<quint32>(x)},
        {"y0", static_cast<quint32>(y)},
        {"w", static_cast<quint32>(width)},
        {"h", static_cast<quint32>(height)},
        {"color", BitParser::colorToBgr(fillColor)},
        {"colorb", BitParser::colorToBgr(strokeColor)},
        {"a", static_cast<quint32>(strokeWidth)},
        {"alph", static_cast<quint32>(qBound(0, qRound(qBound(0, alpha, 255) * 8.0 / 255.0) - 1, 7))}
    };
}

bool RectangleObject::usesAlpha() const
{
    return explicitAlpha || alpha < 255 || fillColor.alpha() < 255;
}
