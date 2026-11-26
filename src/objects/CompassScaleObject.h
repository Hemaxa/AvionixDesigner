// CompassScaleObject — процедурная лента курса с одним 150-битным словом iparam.
#pragma once

#include "BaseObject.h"

// CompassScaleObject — три документированных поля; неописанные биты всегда нулевые.
class CompassScaleObject : public BaseObject
{
    Q_OBJECT
public:
    double positionY = 540.0;
    double courseAngle = 0.0;
    QColor color = QColor("#D3D3D3");

    explicit CompassScaleObject(QObject *parent = nullptr);
    QSharedPointer<BaseObject> clone() const override;
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    QMap<QString, quint32> serializeParams() const override;
    QString getTypeName() const override { return QStringLiteral("CompassScale"); }
    QString getDisplayName() const override { return QStringLiteral("Шкала курса"); }
    QList<QPair<QString, QString>> getProperties() const override;
    bool setObjectProperty(const QString &name, const QString &value) override;
    bool hasValidParameters() const;
    void draw(QPainter &painter) override;
    // Симулятор использует размеры аппаратной сцены, независимо от открытого проекта.
    void drawForWidth(QPainter &painter, int width) const;
    QRectF getBoundingRect() const override;
    QRectF canvasConstraintRect() const override { return QRectF(0, positionY, 1, 1); }
    bool canResize() const override { return false; }
    void moveBy(double dx, double dy) override;

private:
    int canvasWidth() const;
};
