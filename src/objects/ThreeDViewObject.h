//ThreeDViewObject - редакторская заглушка области 3D-вида

#pragma once

#include "BaseObject.h"
#include <QSvgRenderer>

class ThreeDViewObject : public BaseObject
{
    Q_OBJECT

public:
    explicit ThreeDViewObject(QObject *parent = nullptr);

    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override;
    bool canResize() const override { return false; }
    bool contains(const QPointF &point) const override;
    void moveBy(double dx, double dy) override;
    void resizeBy(int edgeFlags, double dx, double dy) override;
    bool setObjectProperty(const QString &name, const QString &value) override;
    QMap<QString, quint32> serializeParams() const override;

private:
    QRectF viewRect() const;

    QSvgRenderer m_placeholderRenderer;
};
