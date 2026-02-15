/**
 * @file RotationObject.cpp
 * @brief Реализация объекта с маской и вращением
 */

#include "RotationObject.h"
#include "BitParser.h"
#include "XmlReader.h"

RotationObject::RotationObject(QObject *parent)
    : BaseObject(parent)
    , color(Qt::white)
{
}

void RotationObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    // Парсинг центра вращения (пиксельные координаты)
    if (schema.contains("xrot")) 
        xRot = BitParser::extract(hexInit, schema["xrot"].offset, schema["xrot"].size);
    if (schema.contains("yrot")) 
        yRot = BitParser::extract(hexInit, schema["yrot"].offset, schema["yrot"].size);

    // Парсинг смещений от точки вращения (знаковые пиксельные расстояния)
    if (schema.contains("top")) 
        top = BitParser::extractSigned(hexInit, schema["top"].offset, schema["top"].size);
    if (schema.contains("left")) 
        left = BitParser::extractSigned(hexInit, schema["left"].offset, schema["left"].size);
    if (schema.contains("bottom")) 
        bottom = BitParser::extractSigned(hexInit, schema["bottom"].offset, schema["bottom"].size);
    if (schema.contains("right")) 
        right = BitParser::extractSigned(hexInit, schema["right"].offset, schema["right"].size);

    // Парсинг цвета
    if (schema.contains("color")) {
        quint32 colorVal = BitParser::extract(hexInit, schema["color"].offset, schema["color"].size);
        color = BitParser::parseColor(colorVal);
    }

    // Парсинг синуса и косинуса (знаковые, с фиксированной точкой xx.xxxxxxxxxxxxxxxx)
    if (schema.contains("sin")) {
        sinVal = BitParser::extractSigned(hexInit, schema["sin"].offset, schema["sin"].size);
    }
    if (schema.contains("cos")) {
        cosVal = BitParser::extractSigned(hexInit, schema["cos"].offset, schema["cos"].size);
    }
}

void RotationObject::parseExtraData(const QDomElement &element)
{
    QDomElement dataEl = element.firstChildElement("data");
    if (dataEl.isNull()) return;

    // Чтение размеров маски
    int w = XmlReader::readInt(dataEl, "width", 0);
    int h = XmlReader::readInt(dataEl, "height", 0);
    QString text = dataEl.text().trimmed();

    if (w <= 0 || h <= 0) return;

    // Создание изображения маски
    maskImage = QImage(w, h, QImage::Format_ARGB32);
    maskImage.fill(Qt::transparent);

    QStringList parts = text.split(',');
    int idx = 0;
    
    // Заполнение пикселей маски
    for (int py = 0; py < h; ++py) {
        for (int px = 0; px < w; ++px) {
            if (idx >= parts.size()) break;
            
            int val = parts[idx].trimmed().toInt();
            idx++;

            if (val > 0) {
                int alpha8bit = (val * 255) / 7;
                QColor pixelColor = color;
                pixelColor.setAlpha(alpha8bit);
                maskImage.setPixelColor(px, py, pixelColor);
            }
        }
    }
}

void RotationObject::draw(QPainter &painter)
{
    if (maskImage.isNull()) return;

    painter.save();
    
    // left/top/right/bottom — смещения от точки вращения (xRot, yRot)
    // Позиция изображения = точка вращения + смещение до левого верхнего угла
    painter.drawImage(QPointF(xRot + left, yRot + top), maskImage);

    painter.restore();
}

QString RotationObject::getTypeName() const
{
    return "RotationObject";
}

QList<QPair<QString, QString>> RotationObject::getProperties() const
{
    QList<QPair<QString, QString>> props = {
        {"X вращения", QString::number(xRot)},
        {"Y вращения", QString::number(yRot)},
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
    
    return props;
}

QRectF RotationObject::getBoundingRect() const
{
    // Абсолютные координаты = точка вращения + смещения
    return QRectF(xRot + left, yRot + top, right - left, bottom - top);
}

bool RotationObject::setObjectProperty(const QString &name, const QString &value)
{
    bool ok = false;
    
    if (name == "Left") {
        left = value.toDouble(&ok);
    } else if (name == "Top") {
        top = value.toDouble(&ok);
    } else if (name == "Right") {
        right = value.toDouble(&ok);
    } else if (name == "Bottom") {
        bottom = value.toDouble(&ok);
    } else if (name == "X вращения") {
        xRot = value.toDouble(&ok);
    } else if (name == "Y вращения") {
        yRot = value.toDouble(&ok);
    } else if (name == "Цвет") {
        color = QColor(value);
        ok = color.isValid();
    }
    
    if (ok) emit changed();
    return ok;
}
