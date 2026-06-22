//DashedLineObject - объект аппаратной штриховой линии

#pragma once

#include "BaseObject.h"

class DashedLineObject : public BaseObject
{
    Q_OBJECT

public:
    bool enabled = true;
    QColor color = QColor("#F8FAFC");
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 80.0;
    double y1 = 0.0;
    int dashPeriod = 16;
    int dashLength = 8;
    int dashPhase = 0;
    int lineWidth = 2;

    explicit DashedLineObject(QObject *parent = nullptr);

    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override;
    bool contains(const QPointF &point) const override;

    void moveBy(double dx, double dy) override;
    void resizeBy(int edgeFlags, double dx, double dy) override;
    bool setObjectProperty(const QString &name, const QString &value) override;
    QMap<QString, quint32> serializeParams() const override;

private:
    int normalizedDashPeriod(int candidate) const;
};
