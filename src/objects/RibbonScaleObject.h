//RibbonScaleObject - объект аппаратной ленточной шкалы

#pragma once

#include "BaseObject.h"

class RibbonScaleObject : public BaseObject
{
    Q_OBJECT

public:
    bool enabled = true;
    QColor color = QColor("#8DE1FF");
    double left = 0.0;
    double right = 160.0;
    double top = 0.0;
    double bottom = 220.0;
    int lineWidth = 2;
    int period = 24;
    double yStart = 0.0;

    explicit RibbonScaleObject(QObject *parent = nullptr);

    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override;

    void moveBy(double dx, double dy) override;
    void resizeBy(int edgeFlags, double dx, double dy) override;
    bool setObjectProperty(const QString &name, const QString &value) override;
    QMap<QString, quint32> serializeParams() const override;
};
