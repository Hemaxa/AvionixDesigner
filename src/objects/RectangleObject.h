/**
 * @file RectangleObject.h
 * @brief Прямоугольный объект с заливкой и обводкой
 */

#pragma once

#include "BaseObject.h"

/**
 * @class RectangleObject
 * @brief Прямоугольник с заливкой, обводкой и опциональной прозрачностью
 */
class RectangleObject : public BaseObject
{
    Q_OBJECT
    
public:
    double x = 0;            // Координата X
    double y = 0;            // Координата Y
    double width = 0;        // Ширина
    double height = 0;       // Высота
    QColor fillColor;        // Цвет заливки
    QColor strokeColor;      // Цвет обводки
    double strokeWidth = 0;  // Толщина обводки
    int alpha = 255;         // Прозрачность
    
    explicit RectangleObject(QObject *parent = nullptr);
    
    // Парсит параметры из HEX-строки
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    
    // Отрисовывает прямоугольник
    void draw(QPainter &painter) override;
    
    // Возвращает имя типа
    QString typeName() const override;
    
    // Возвращает свойства для отображения
    QList<QPair<QString, QString>> getProperties() const override;
    
    // Возвращает ограничивающий прямоугольник
    QRectF getBoundingRect() const override;
};
