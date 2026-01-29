/**
 * @file StaticGroupObject.cpp
 * @brief Реализация статической группы
 */

#include "StaticGroupObject.h"
#include "../utils/BitParser.h"

StaticGroupObject::StaticGroupObject(QObject *parent)
    : AbstractObject(parent)
    , color(Qt::gray)
{
}

void StaticGroupObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    if (schema.contains("x")) 
        x = BitParser::extract(hexInit, schema["x"].offset, schema["x"].size) / 10.0;
    if (schema.contains("y")) 
        y = BitParser::extract(hexInit, schema["y"].offset, schema["y"].size) / 10.0;
    if (schema.contains("w"))  
        width = BitParser::extract(hexInit, schema["w"].offset, schema["w"].size) / 10.0;
    if (schema.contains("h"))  
        height = BitParser::extract(hexInit, schema["h"].offset, schema["h"].size) / 10.0;

    if (schema.contains("color")) {
        quint32 colorVal = BitParser::extract(hexInit, schema["color"].offset, schema["color"].size);
        color = BitParser::parseColor(colorVal);
    }
    
    if (schema.contains("addr")) {
        address = BitParser::extract(hexInit, schema["addr"].offset, schema["addr"].size);
    }
}

void StaticGroupObject::draw(QPainter &painter)
{
    painter.save();
    
    QPen pen(Qt::gray, 1, Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(color.lighter(150));
    painter.drawRect(QRectF(x, y, width, height));
    
    painter.setPen(Qt::darkGray);
    painter.drawText(QRectF(x, y, width, height), Qt::AlignCenter, 
                     QString("SG#%1").arg(groupNumber));
    
    painter.restore();
}

QString StaticGroupObject::typeName() const
{
    return "StaticGroup";
}

QList<QPair<QString, QString>> StaticGroupObject::getProperties() const
{
    return {
        {"X", QString::number(x, 'f', 1)},
        {"Y", QString::number(y, 'f', 1)},
        {"Ширина", QString::number(width, 'f', 1)},
        {"Высота", QString::number(height, 'f', 1)},
        {"Цвет", color.name()},
        {"Адрес", QString::number(address)},
        {"Номер группы", QString::number(groupNumber)}
    };
}

QRectF StaticGroupObject::getBoundingRect() const
{
    return QRectF(x, y, width, height);
}
