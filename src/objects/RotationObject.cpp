#include "RotationObject.h"
#include "BitParser.h"
#include "XmlReader.h"
#include <QtMath>

RotationObject::RotationObject(QObject *parent) : BaseObject(parent), color(Qt::white) {}

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

    //создание изображения маски
    maskImage = QImage(w, h, QImage::Format_ARGB32);
    maskImage.fill(Qt::transparent); //сначала изображение прозрачное

    QStringList parts = text.split(',');
    int idx = 0;
    
    //заполнение пикселей маски
    for (int py = 0; py < h; ++py) {
        for (int px = 0; px < w; ++px) {
            if (idx >= parts.size()) break;
            
            int val = parts[idx].trimmed().toInt();
            idx++;

            //в xml содержатся значения 0..7, 0 - полностью прозрачно, 7 - совершенно непрозрачно
            if (val > 0) {
                int alpha8bit = (val * 255) / 7;
                QColor pixelColor = color; //цвет берется из цвета объекта
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
    
    //вычисляем угол вращения из sin/cos
    double angleDeg = getAngleDegrees();
    
    if (qAbs(angleDeg) > 0.01) {
        //переносим начало координат в точку вращения, поворачиваем, возвращаем
        painter.translate(xRot, yRot);
        painter.rotate(angleDeg);
        painter.drawImage(QPointF(left, top), maskImage);
    }
    else {
        //без вращения — рисуем напрямую
        painter.drawImage(QPointF(xRot + left, yRot + top), maskImage);
    }
    painter.restore();
}

QString RotationObject::getTypeName() const
{
    return "RotationObject";
}

QString RotationObject::getDisplayName() const
{
    return "Rotation Group";
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
    
    return props;
}

QRectF RotationObject::getBoundingRect() const
{
    //абсолютные координаты = точка вращения + смещения
    return QRectF(xRot + left, yRot + top, right - left, bottom - top);
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
    emit changed();
}

void RotationObject::resizeBy(int edgeFlags, double dx, double dy)
{
    const double oldWidth = qMax(1.0, right - left);
    const double oldHeight = qMax(1.0, bottom - top);

    // Так как размеры хранятся как смещения (top, left, bottom, right),
    // нам нужно учитывать текущий поворот.
    // Проще всего конвертировать dx, dy в локальные координаты.
    double angleRad = qDegreesToRadians(-getAngleDegrees()); // Обратный поворот
    double localDx = dx * qCos(angleRad) - dy * qSin(angleRad);
    double localDy = dx * qSin(angleRad) + dy * qCos(angleRad);
    
    if (edgeFlags & 1) { // Left
        left += localDx;
        if (left >= right) left = right - 1;
    }
    if (edgeFlags & 2) { // Right
        right += localDx;
        if (right <= left) right = left + 1;
    }
    if (edgeFlags & 4) { // Top
        top += localDy;
        if (top >= bottom) top = bottom - 1;
    }
    if (edgeFlags & 8) { // Bottom
        bottom += localDy;
        if (bottom <= top) bottom = top + 1;
    }

    const int newWidth = qMax(1, qRound(right - left));
    const int newHeight = qMax(1, qRound(bottom - top));
    if (!maskImage.isNull() && (qRound(oldWidth) != newWidth || qRound(oldHeight) != newHeight)) {
        maskImage = maskImage.scaled(newWidth, newHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    
    emit changed();
}

void RotationObject::setRotation(double angle)
{
    double angleRad = qDegreesToRadians(angle);
    sinVal = qRound(qSin(angleRad) * 65536.0);
    cosVal = qRound(qCos(angleRad) * 65536.0);
    emit changed();
}

bool RotationObject::setObjectProperty(const QString &name, const QString &value)
{
    bool ok = false;
    
    if (name == "Left" || name == "Left (смещ.)") {
        left = value.toDouble(&ok);
    }
    else if (name == "Top" || name == "Top (смещ.)") {
        top = value.toDouble(&ok);
    }
    else if (name == "Right" || name == "Right (смещ.)") {
        right = value.toDouble(&ok);
    }
    else if (name == "Bottom" || name == "Bottom (смещ.)") {
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
    
    if (ok) emit changed();
    return ok;
}

QMap<QString, quint32> RotationObject::serializeParams() const
{
    const int width = qMax(1, qRound(right - left));
    return {
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
