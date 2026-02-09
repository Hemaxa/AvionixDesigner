/**
 * @file RotationObject.h
 * @brief Растровый объект с маской и поддержкой вращения
 */

#pragma once

#include "BaseObject.h"
#include <QImage>

/**
 * @class RotationObject
 * @brief Объект с растровой маской и вращением
 */
class RotationObject : public BaseObject
{
    Q_OBJECT
    
public:
    double left = 0;     // Левая граница
    double top = 0;      // Верхняя граница
    double right = 0;    // Правая граница
    double bottom = 0;   // Нижняя граница
    double xRot = 0;     // Центр вращения X
    double yRot = 0;     // Центр вращения Y
    double sinVal = 0;   // Синус угла
    double cosVal = 0;   // Косинус угла
    QColor color;        // Цвет объекта
    QImage maskImage;    // Растровая маска
    
    explicit RotationObject(QObject *parent = nullptr);
    
    // Парсит параметры из HEX-строки
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    
    // Парсит дополнительные данные (маска)
    void parseExtraData(const QDomElement &element) override;
    
    // Отрисовывает объект
    void draw(QPainter &painter) override;
    
    // Возвращает имя типа
    QString typeName() const override;
    
    // Возвращает свойства
    QList<QPair<QString, QString>> getProperties() const override;
    
    // Возвращает ограничивающий прямоугольник
    QRectF getBoundingRect() const override;
};
