//ThreeDViewObject - область 3D-вида и положение самолета для ПЛИС

#pragma once

#include "BaseObject.h"
#include <QSvgRenderer>
#include "fpga/model/Layer3dFormat.h"

class ThreeDViewObject : public BaseObject
{
    Q_OBJECT

public:
    explicit ThreeDViewObject(QObject *parent = nullptr);

    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void parseExtraData(const QDomElement &element) override;
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

    int aircraftX() const;
    int aircraftY() const;
    double aircraftAltitude() const;
    int aircraftCourse() const;
    int aircraftPitch() const;
    bool setAircraftState(int x, int y, double altitude, int course, int pitch);
    QString serializeInit() const;
    QRectF aircraftMarkerRect() const;
    bool isLeftAnchored() const { return m_leftAnchored; }
    void setLeftAnchored(bool left) { m_leftAnchored = left; }

private:
    QRectF viewRect() const;
    QFont annotationFont() const;

    // Keep raw fields (including fractional course/height and reserved bits)
    // intact until the corresponding control is changed.
    QString m_initHex = QString(Layer3dFormat::hexLength, QLatin1Char('0'));
    QSvgRenderer m_markerRenderer;
    bool m_leftAnchored = false;
};
