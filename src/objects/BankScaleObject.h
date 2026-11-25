// BankScaleObject — параметрическая шкала крена без растровой памяти ПЛИС.
#pragma once

#include "BaseObject.h"
#include "fpga/model/BankScaleFormat.h"

// BankScaleObject — одно слово BANK_SCALE; рисунок общий для редактора и симулятора.
class BankScaleObject : public BaseObject
{
    Q_OBJECT
public:
    bool enabled = true;
    double centerX = 960.0;
    double centerY = 540.0;
    double bankAngle = 0.0;
    // Сохраняем поле протокола, но не даём менять размер, пока его не принимает плата.
    int radius = BankScaleFormat::fixedRadius;
    QColor tickColor = QColor("#D3D3D3");
    QColor mark20Color = QColor("#9C4868");
    bool colorValid = true;

    explicit BankScaleObject(QObject *parent = nullptr);
    QSharedPointer<BaseObject> clone() const override;
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override { return QStringLiteral("BankScale"); }
    QString getDisplayName() const override { return QStringLiteral("Шкала крена"); }
    QList<QPair<QString, QString>> getProperties() const override;
    QMap<QString, quint32> serializeParams() const override;
    bool setObjectProperty(const QString &name, const QString &value) override;
    bool hasValidParameters() const;

    QRectF getBoundingRect() const override;
    QRectF canvasConstraintRect() const override { return QRectF(centerX, centerY, 1, 1); }
    QPolygonF selectionPolygon() const override;
    bool contains(const QPointF &point) const override;
    bool canResize() const override { return false; }
    bool supportsRotationHandle() const override { return true; }
    QTransform rotationFrame() const override;
    QPointF rotationCenter() const override;
    void moveBy(double dx, double dy) override;
    void setRotation(double angle) override;

private:
    QTransform drawingTransform() const;
};
