//AviaHorizonObject - объект авиагоризонта с закраской неба, земли и линии горизонта

#pragma once

#include "BaseObject.h"

class AviaHorizonObject : public BaseObject
{
    Q_OBJECT

public:
    bool enabled = true;
    QColor earthColor = QColor("#c27d1b");
    QColor skyColor = QColor("#4fcaf7");
    QColor horizonLineColor = QColor("#ffffc0");
    double lineWidth = 4.0;
    double xCenter = 0.0;
    double yCenter = 0.0;
    double areaWidth = 220.0;
    double areaHeight = 220.0;
    double sinVal = 0.0;
    double cosVal = 65536.0;

    explicit AviaHorizonObject(QObject *parent = nullptr);

    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override;
    bool supportsRotationHandle() const override { return true; }

    void moveBy(double dx, double dy) override;
    void resizeBy(int edgeFlags, double dx, double dy) override;
    void setRotation(double angle) override;
    bool setObjectProperty(const QString &name, const QString &value) override;
    QMap<QString, quint32> serializeParams() const override;
    bool contains(const QPointF &point) const override;

    double getAngleDegrees() const;

private:
    QRectF getLocalAreaRect() const;
};
