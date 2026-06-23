//ImageObject - редактируемый источник изображения до экспорта в растровую маску ПЛИС

#pragma once

#include "BaseObject.h"

#include <QByteArray>
#include <QImage>

class ImageObject : public BaseObject
{
    Q_OBJECT

public:
    double x = 0.0;
    double y = 0.0;
    double width = 1.0;
    double height = 1.0;
    QColor maskColor = QColor("#FFFFFF");
    QString sourceName;
    QString format = QStringLiteral("raster");

    explicit ImageObject(QObject *parent = nullptr);

    void setRasterImage(const QImage &image, const QString &name);
    void setSvgData(const QByteArray &data, const QString &name, const QSize &defaultSize);
    QImage renderedImage() const;
    QByteArray sourcePayload() const;
    void setSourcePayload(const QByteArray &payload);

    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override;

    void moveBy(double dx, double dy) override;
    void resizeBy(int edgeFlags, double dx, double dy) override;
    bool setObjectProperty(const QString &name, const QString &value) override;

private:
    QImage m_sourceImage;
    QByteArray m_sourceBytes;
};
