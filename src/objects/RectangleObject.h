/**
 * @file RectangleObject.h
 * @brief Прямоугольный объект с заливкой и обводкой
 */

#pragma once

#include "AbstractObject.h"

/**
 * @class RectangleObject
 * @brief Прямоугольник с заливкой, обводкой и опциональной прозрачностью
 */
class RectangleObject : public AbstractObject
{
    Q_OBJECT
    
public:
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
    QColor fillColor;
    QColor strokeColor;
    double strokeWidth = 0;
    int alpha = 255;
    
    explicit RectangleObject(QObject *parent = nullptr);
    
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString typeName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override;
};
