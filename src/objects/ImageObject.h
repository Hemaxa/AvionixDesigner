//ImageObject - редактируемый источник изображения до экспорта в растровую маску ПЛИС

#pragma once

#include "BaseObject.h"

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QRect>

struct ImageColorLayer
{
    QColor sourceColor;
    QColor maskColor;
    bool autoMaskColor = true;
};

struct ImageMaskComponent
{
    int layerIndex = 0;
    QRect bounds;
    QColor color;
    QImage mask;
};

class ImageObject : public BaseObject
{
    Q_OBJECT

public:
    double x = 0.0;
    double y = 0.0;
    double width = 1.0;
    double height = 1.0;
    QColor maskColor = QColor("#FFFFFF");
    bool autoMaskColor = true;
    double rotationDegrees = 0.0;
    QString sourceName;
    QString format = QStringLiteral("raster");

    explicit ImageObject(QObject *parent = nullptr);

    void setRasterImage(const QImage &image, const QString &name);
    void setSvgData(const QByteArray &data, const QString &name, const QSize &defaultSize);
    QImage renderedSourceImage() const;
    QImage renderedImage() const;
    QColor effectiveMaskColor() const;
    QList<ImageMaskComponent> maskComponents() const;
    QList<QList<ImageMaskComponent>> maskComponentGroups() const;
    QList<ImageColorLayer> colorLayers() const;
    void setColorLayers(const QList<ImageColorLayer> &layers);
    bool hasRotation() const;
    QByteArray sourcePayload() const;
    void setSourcePayload(const QByteArray &payload);

    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override;
    bool supportsRotationHandle() const override { return canResize(); }
    bool contains(const QPointF &point) const override;

    void moveBy(double dx, double dy) override;
    void resizeBy(int edgeFlags, double dx, double dy) override;
    void setRotation(double angle) override;
    bool setObjectProperty(const QString &name, const QString &value) override;

private:
    void rebuildColorLayers();
    QImage m_sourceImage;
    QByteArray m_sourceBytes;
    QList<ImageColorLayer> m_colorLayers;
};
