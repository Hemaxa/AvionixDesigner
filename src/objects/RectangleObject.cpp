#include "RectangleObject.h"
#include "BitParser.h"

RectangleObject::RectangleObject(QObject *parent) : BaseObject(parent), fillColor(Qt::white), strokeColor(Qt::transparent) {}

void RectangleObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    //парсинг координат
    //если в XML сказано, что у прямоугольника есть параметр "x0"
    if (schema.contains("x0")) 
    // BitParser вырезает биты по инструкции и мы делим на 10.0 (фиксированная точка)
        x = BitParser::extract(hexInit, schema["x0"].offset, schema["x0"].size);
    if (schema.contains("y0")) 
        y = BitParser::extract(hexInit, schema["y0"].offset, schema["y0"].size);
    if (schema.contains("w"))  
        width = BitParser::extract(hexInit, schema["w"].offset, schema["w"].size);
    if (schema.contains("h"))  
        height = BitParser::extract(hexInit, schema["h"].offset, schema["h"].size);

    // Парсинг цвета заливки
    if (schema.contains("color")) {
        quint32 colorVal = BitParser::extract(hexInit, schema["color"].offset, schema["color"].size);
        fillColor = BitParser::parseColor(colorVal);
    }

    // Парсинг цвета обводки
    if (schema.contains("colorb")) {
        quint32 colorVal = BitParser::extract(hexInit, schema["colorb"].offset, schema["colorb"].size);
        strokeColor = BitParser::parseColor(colorVal);
    } else {
        strokeColor = Qt::transparent;
    }

    // Парсинг толщины обводки
    if (schema.contains("a")) {
        strokeWidth = BitParser::extract(hexInit, schema["a"].offset, schema["a"].size);
    }
    
    // Парсинг прозрачности
    if (schema.contains("alph")) {
        int rawAlpha = BitParser::extract(hexInit, schema["alph"].offset, schema["alph"].size);
        alpha = qMin(255, rawAlpha * 4);
        fillColor.setAlpha(alpha);
    }
}

void RectangleObject::draw(QPainter &painter)
{
    QPen pen;
    // Настройка пера для обводки
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

QString RectangleObject::getTypeName() const
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

bool RectangleObject::setObjectProperty(const QString &name, const QString &value)
{
    bool ok = false;
    
    if (name == "X") {
        x = value.toDouble(&ok);
    } else if (name == "Y") {
        y = value.toDouble(&ok);
    } else if (name == "Ширина") {
        width = value.toDouble(&ok);
    } else if (name == "Высота") {
        height = value.toDouble(&ok);
    } else if (name == "Заливка") {
        fillColor = QColor(value);
        ok = fillColor.isValid();
    } else if (name == "Обводка") {
        strokeColor = QColor(value);
        ok = strokeColor.isValid();
    } else if (name == "Толщина") {
        strokeWidth = value.toDouble(&ok);
    } else if (name == "Прозрачность") {
        alpha = value.toInt(&ok);
        if (ok) fillColor.setAlpha(alpha);
    }
    
    if (ok) emit changed();
    return ok;
}
