// RotationObject - класс растрового объекта с маской и поддержкой вращения.

#pragma once

#include "BaseObject.h"
#include <QImage>
#include <QStringList>
#include <QVector>

// RotationObjectLayer - параметры одного аппаратного слоя вращающейся маски.
struct RotationObjectLayer
{
    double left = 0;
    double top = 0;
    double right = 0;
    double bottom = 0;
    double xRot = 0;
    double yRot = 0;
    double sinVal = 0;
    double cosVal = 0;
    QColor color = Qt::white;
    bool enabled = true;
};

// RotationObject - класс растрового объекта с маской и поддержкой вращения.
class RotationObject : public BaseObject
{
    Q_OBJECT

public:
    bool enabled = true;
    // параметры объекта
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

    // переопределение виртуальных методов из базового класса
    // Независимая копия параметров, ресурсов и состояния редактора.
    QSharedPointer<BaseObject> clone() const override;

    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override;
    bool supportsRotationHandle() const override { return true; }
    QTransform rotationFrame() const override;
    QPointF rotationCenter() const override;

    // Взаимодействие
    bool contains(const QPointF &point) const override;
    void moveBy(double dx, double dy) override;
    void resizeBy(int edgeFlags, double dx, double dy, bool proportional = false) override;
    void setRotation(double angle) override;

    bool setObjectProperty(const QString &name, const QString &value) override;
    QMap<QString, quint32> serializeParams() const override;
    QList<QMap<QString, quint32>> serializeLayerParams() const;
    void setHardwareLayersFromInitHex(const QStringList &initHexList, const ParamSchema &schema);
    QVector<RotationObjectLayer> hardwareLayers() const;
    void restoreHardwareLayers(const QVector<RotationObjectLayer> &layers) { m_layers = layers; }

    // метод, который возвращает угол вращения в градусах, вычисленный из sinVal/cosVal
    double getAngleDegrees() const;

private:
    RotationObjectLayer currentLayer() const;
    void applyLayerToPublicFields(const RotationObjectLayer &layer);
    void syncFirstLayerFromPublicFields();

    QVector<RotationObjectLayer> m_layers;
};
