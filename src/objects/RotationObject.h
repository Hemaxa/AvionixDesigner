/**
 * @file RotationObject.h
 * @brief Растровый объект с маской и поддержкой вращения
 */

#pragma once

#include "AbstractObject.h"
#include <QImage>

/**
 * @class RotationObject
 * @brief Объект с растровой маской и вращением
 */
class RotationObject : public AbstractObject
{
    Q_OBJECT
    
public:
    double left = 0;
    double top = 0;
    double right = 0;
    double bottom = 0;
    double xRot = 0;
    double yRot = 0;
    double sinVal = 0;
    double cosVal = 0;
    QColor color;
    QImage maskImage;
    
    explicit RotationObject(QObject *parent = nullptr);
    
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void parseExtraData(const QDomElement &element) override;
    void draw(QPainter &painter) override;
    QString typeName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override;
};
