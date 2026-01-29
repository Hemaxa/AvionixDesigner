/**
 * @file RectangleObject.cpp
 * @brief Реализация прямоугольного объекта
 */

#include "RectangleObject.h"
#include "../utils/BitParser.h"

RectangleObject::RectangleObject(QObject *parent)
    : AbstractObject(parent)
    , fillColor(Qt::white)
    , strokeColor(Qt::transparent)
{
}

void RectangleObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    if (schema.contains("x0")) 
        x = BitParser::extract(hexInit, schema["x0"].offset, schema["x0"].size) / 10.0;
    if (schema.contains("y0")) 
        y = BitParser::extract(hexInit, schema["y0"].offset, schema["y0"].size) / 10.0;
    if (schema.contains("w"))  
        width = BitParser::extract(hexInit, schema["w"].offset, schema["w"].size) / 10.0;
    if (schema.contains("h"))  
        height = BitParser::extract(hexInit, schema["h"].offset, schema["h"].size) / 10.0;

    if (schema.contains("color")) {
        quint32 colorVal = BitParser::extract(hexInit, schema["color"].offset, schema["color"].size);
        fillColor = BitParser::parseColor(colorVal);
    }

    if (schema.contains("colorb")) {
        quint32 colorVal = BitParser::extract(hexInit, schema["colorb"].offset, schema["colorb"].size);
        strokeColor = BitParser::parseColor(colorVal);
    } else {
        strokeColor = Qt::transparent;
    }

    if (schema.contains("a")) {
        strokeWidth = BitParser::extract(hexInit, schema["a"].offset, schema["a"].size) / 10.0;
    }
    
    if (schema.contains("alph")) {
        int rawAlpha = BitParser::extract(hexInit, schema["alph"].offset, schema["alph"].size);
        alpha = qMin(255, rawAlpha * 4);
        fillColor.setAlpha(alpha);
    }
}

void RectangleObject::draw(QPainter &painter)
{
    QPen pen;
    if (strokeWidth <= 0.05 || strokeColor.alpha() == 0) {
        pen.setStyle(Qt::NoPen);
    } else {
        pen.setColor(strokeColor);
        pen.setWidthF(strokeWidth);
    }
    painter.setPen(pen);
    painter.setBrush(fillColor);
    painter.drawRect(QRectF(x, y, width, height));
}

QString RectangleObject::typeName() const
{
    return "Rectangle";
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
        {"Прозрачность", QString::number(alpha)}
    };
}

QRectF RectangleObject::getBoundingRect() const
{
    return QRectF(x, y, width, height);
}
