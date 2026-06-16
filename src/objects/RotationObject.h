//RotationObject - класс растрового объекта с маской и поддержкой вращения

#pragma once

#include "BaseObject.h"
#include <QImage>

class RotationObject : public BaseObject
{
    Q_OBJECT
    
public:
    //параметры объекта
    double left = 0; //левая граница
    double top = 0; //верхняя граница
    double right = 0; //правая граница
    double bottom = 0; //нижняя граница
    double xRot = 0; //центр вращения X
    double yRot = 0; //центр вращения Y
    double sinVal = 0; //синус угла
    double cosVal = 0; //косинус угла
    QColor color; //цвет объекта
    QImage maskImage; //растровая маска
    
    explicit RotationObject(QObject *parent = nullptr);
    
    //переопределение виртуальных методов из базового класса
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override;
    bool supportsRotationHandle() const override { return true; }
    
    // Взаимодействие
    bool contains(const QPointF &point) const override;
    void moveBy(double dx, double dy) override;
    void resizeBy(int edgeFlags, double dx, double dy) override;
    void setRotation(double angle) override;
    
    bool setObjectProperty(const QString &name, const QString &value) override;
    void parseExtraData(const QDomElement &element) override;
    QMap<QString, quint32> serializeParams() const override;
    
    //метод, который возвращает угол вращения в градусах, вычисленный из sinVal/cosVal
    double getAngleDegrees() const;
};
