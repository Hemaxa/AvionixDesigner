#ifndef SHAPES_H
#define SHAPES_H

#include <QPainter>
#include <QMap>
#include <QImage>
#include <QDomElement>
#include "utils.h"

// --- Базовый абстрактный класс фигуры ---
class CorelShape {
public:
    virtual ~CorelShape() {}
    
    // Метод парсинга данных из Hex-строки и схемы
    virtual void parse(const QString &hexInit, const QMap<QString, ParamInfo> &schema) = 0;
    
    // Метод отрисовки
    virtual void draw(QPainter &painter) = 0;
    
    // Метод для доп. данных (например, <data> для RotationObject)
    virtual void parseExtraData(const QDomElement &element) { Q_UNUSED(element); }
};

// --- Класс Прямоугольника (поддерживает и обычный, и с альфа-каналом) ---
class CorelRect : public CorelShape {
public:
    double x, y, w, h;
    QColor color;
    QColor strokeColor;
    double strokeWidth;
    int alpha; // Прозрачность (0-255)

    CorelRect() : alpha(255) {}

    void parse(const QString &hexInit, const QMap<QString, ParamInfo> &schema) override {
        // Координаты
        if (schema.contains("x0")) x = BitParser::extract(hexInit, schema["x0"].offset, schema["x0"].size) / 10.0;
        if (schema.contains("y0")) y = BitParser::extract(hexInit, schema["y0"].offset, schema["y0"].size) / 10.0;
        if (schema.contains("w"))  w = BitParser::extract(hexInit, schema["w"].offset, schema["w"].size) / 10.0;
        if (schema.contains("h"))  h = BitParser::extract(hexInit, schema["h"].offset, schema["h"].size) / 10.0;

        // Цвет
        if (schema.contains("color")) {
            color = BitParser::parseColor(BitParser::extract(hexInit, schema["color"].offset, schema["color"].size));
        }

        // Обводка
        if (schema.contains("colorb")) {
            strokeColor = BitParser::parseColor(BitParser::extract(hexInit, schema["colorb"].offset, schema["colorb"].size));
        } else {
            strokeColor = Qt::transparent;
        }

        if (schema.contains("a")) {
            strokeWidth = BitParser::extract(hexInit, schema["a"].offset, schema["a"].size) / 10.0;
        } else {
            strokeWidth = 0;
        }
        
        // --- Поддержка RECTANGLEa (Альфа-канал) ---
        // Обычно параметр называется "alph" или "alpha"
        if (schema.contains("alph")) {
            // В скрипте формула: (alph * 5 + 32) \ 64. 
            // Это квантование. Мы просто возьмем сырое значение и масштабируем к 0-255.
            // Предположим, что там значение 0-63 или около того.
            int rawAlpha = BitParser::extract(hexInit, schema["alph"].offset, schema["alph"].size);
            // Если rawAlpha ~ 0..31 или 0..63, нужно привести к 0..255.
            // Пока сделаем так (предполагая 5-6 бит):
            alpha = rawAlpha * 4; // Примерная подгонка, нужно уточнить по размеру поля
            if (alpha > 255) alpha = 255;
            
            color.setAlpha(alpha);
        }
    }

    void draw(QPainter &painter) override {
        QPen pen;
        if (strokeWidth <= 0.05 || strokeColor.alpha() == 0) {
            pen.setStyle(Qt::NoPen);
        } else {
            pen.setColor(strokeColor);
            pen.setWidthF(strokeWidth);
        }
        painter.setPen(pen);
        painter.setBrush(color);
        painter.drawRect(QRectF(x, y, w, h));
    }
};

// --- Класс RotationObject (Сложный объект с маской) ---
class CorelRotationObject : public CorelShape {
public:
    double top, left, bottom, right; // Габариты
    double xRot, yRot; // Центр вращения
    double sinVal, cosVal; // Угол
    QColor color;
    QImage maskImage; // Сгенерированная маска

    void parse(const QString &hexInit, const QMap<QString, ParamInfo> &schema) override {
        // Габариты (делим на 10.0)
        if (schema.contains("left")) left = BitParser::extract(hexInit, schema["left"].offset, schema["left"].size) / 10.0;
        if (schema.contains("top")) top = BitParser::extract(hexInit, schema["top"].offset, schema["top"].size) / 10.0;
        if (schema.contains("right")) right = BitParser::extract(hexInit, schema["right"].offset, schema["right"].size) / 10.0;
        if (schema.contains("bottom")) bottom = BitParser::extract(hexInit, schema["bottom"].offset, schema["bottom"].size) / 10.0;
        
        // Центр вращения
        if (schema.contains("xrot")) xRot = BitParser::extract(hexInit, schema["xrot"].offset, schema["xrot"].size) / 10.0;
        if (schema.contains("yrot")) yRot = BitParser::extract(hexInit, schema["yrot"].offset, schema["yrot"].size) / 10.0;

        // Цвет заливки
        if (schema.contains("color")) {
            color = BitParser::parseColor(BitParser::extract(hexInit, schema["color"].offset, schema["color"].size));
        }

        // Угол (Sin/Cos). 
        // В доке: 18 бит, знаковое, фиксированная точка. 
        // Пока реализуем упрощенно: считаем их double-ом.
        // Для точного поворота лучше использовать xRot/yRot и координаты углов, но пока считаем угол.
        if (schema.contains("sin")) {
            quint32 rawSin = BitParser::extract(hexInit, schema["sin"].offset, schema["sin"].size);
            // Тут нужна спец. логика для знаковых 18-битных чисел, пока оставим заглушку
            sinVal = rawSin; 
        }
        if (schema.contains("cos")) {
            quint32 rawCos = BitParser::extract(hexInit, schema["cos"].offset, schema["cos"].size);
            cosVal = rawCos;
        }
    }

    // Парсинг маски из тега <data>
    void parseExtraData(const QDomElement &element) override {
        QDomElement dataEl = element.firstChildElement("data");
        if (dataEl.isNull()) return;

        int w = dataEl.attribute("width").toInt();
        int h = dataEl.attribute("height").toInt();
        QString text = dataEl.text().trimmed(); // "0, 0, 7, 7, ..."

        if (w <= 0 || h <= 0) return;

        maskImage = QImage(w, h, QImage::Format_ARGB32);
        maskImage.fill(Qt::transparent);

        // Разбиваем CSV текст на числа
        QStringList parts = text.split(',');
        int idx = 0;
        
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (idx >= parts.size()) break;
                
                // Значение альфа (0..7)
                int val = parts[idx].trimmed().toInt();
                idx++;

                if (val > 0) {
                    // Масштабируем 0..7 -> 0..255
                    int alpha8bit = (val * 255) / 7;
                    QColor pixelColor = color;
                    pixelColor.setAlpha(alpha8bit);
                    maskImage.setPixelColor(x, y, pixelColor);
                }
            }
        }
    }

    void draw(QPainter &painter) override {
        if (maskImage.isNull()) return;

        painter.save(); // Сохраняем состояние (поворот, смещение)

        // 1. Смещаем координаты к точке вращения
        painter.translate(xRot, yRot);
        
        // 2. Вращаем (здесь нужно корректно высчитать угол из sin/cos)
        // Пока для теста без вращения или простой atan2, если значения расшифрованы верно
        // double angleRad = qAtan2(sinVal, cosVal);
        // painter.rotate(qRadiansToDegrees(angleRad));

        // 3. Возвращаем смещение назад
        painter.translate(-xRot, -yRot);

        // 4. Рисуем маску
        // Важно: координаты left/top в RotationObject обычно указывают, где рисовать картинку
        painter.drawImage(QPointF(left, top), maskImage);

        painter.restore();
    }
};

#endif // SHAPES_H