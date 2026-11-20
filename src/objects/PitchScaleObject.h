// PitchScaleObject — параметрическая шкала тангажа без растровой памяти ПЛИС.
#pragma once

#include "BaseObject.h"
#include <QImage>

// PitchScaleObject — одно слово iparam и общий приближённый рисунок редактора/симулятора.
class PitchScaleObject : public BaseObject
{
    Q_OBJECT
public:
    bool enabled = true;
    int scaleMode = 0;
    double centerX = 960.0;
    double centerY = 540.0;
    double pitchAngle = 0.0;
    double rotationAngle = 0.0;

    explicit PitchScaleObject(QObject *parent = nullptr);
    QSharedPointer<BaseObject> clone() const override;
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override { return QStringLiteral("PitchScale"); }
    QString getDisplayName() const override { return QStringLiteral("Шкала тангажа"); }
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

    // Знак соответствует экранной оси Y: положительная разность углов идёт вверх.
    static double tickOffset(double deltaDegrees, int mode);

private:
    void updatePreview() const;
    QTransform drawingTransform() const;
    mutable QImage m_preview;
    mutable QRect m_localBounds;
    mutable int m_cachedPitch = 0;
    mutable int m_cachedMode = -1;
};
