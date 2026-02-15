//RectangleObject - класс прямоугольного объекта с заливкой и обводкой

#pragma once

#include "BaseObject.h"

//наследуемся от базового класса объекта
class RectangleObject : public BaseObject
{
    Q_OBJECT
    
public:
    //параметры объекта прямоугольник
    double x = 0; //координата X
    double y = 0; //координата Y
    double width = 0; //ширина
    double height = 0; //высота
    QColor fillColor; //цвет заливки
    QColor strokeColor; //цвет обводки
    double strokeWidth = 0; //толщина обводки
    int alpha = 255; //прозрачность
    
    explicit RectangleObject(QObject *parent = nullptr);
    
    //переопределение виртуальных методов из базового класса
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    bool setObjectProperty(const QString &name, const QString &value) override;
    QRectF getBoundingRect() const override;
};
