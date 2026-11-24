// ImageObject - редактируемый источник изображения до экспорта в растровую маску ПЛИС.

#pragma once

#include "BaseObject.h"

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QRect>

// ImageColorLayer - исходный цвет слоя и выбранный цвет аппаратной маски.
struct ImageColorLayer
{
    QColor sourceColor;
    QColor maskColor;
    bool autoMaskColor = true;
    // Границы исходной области в долях изображения; цвет и порядок не меняют её идентичность.
    QRectF sourceBounds;
    int order = -1;
    bool operator==(const ImageColorLayer &other) const
    {
        return sourceColor == other.sourceColor && maskColor == other.maskColor &&
               autoMaskColor == other.autoMaskColor && sourceBounds == other.sourceBounds && order == other.order;
    }
};

// ImageMaskComponent - цветовая маска изображения со своим цветом и габаритами.
struct ImageMaskComponent
{
    int layerIndex = 0;
    QRect bounds;
    QColor color;
    QImage mask;
};

// ImageObject - редактируемый источник изображения до экспорта в растровую маску ПЛИС.
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
    // Редакторские отражения исходника до штатной сборки цветовых масок.
    bool mirrorHorizontal = false;
    bool mirrorVertical = false;
    QString sourceName;
    QString format = QStringLiteral("raster");

    // Kind - фиксированный тип изображения: статический или вращающийся.
    enum class Kind { Static, Rotating };
    explicit ImageObject(Kind kind = Kind::Static, QObject *parent = nullptr);
    bool isRotationObject() const { return m_kind == Kind::Rotating; }

    void setRasterImage(const QImage &image, const QString &name);
    void setSvgData(const QByteArray &data, const QString &name, const QSize &defaultSize);
    QImage renderedSourceImage() const;
    QImage renderedImage() const;
    // Версия локального растра: перемещение и поворот готового изображения её не меняют.
    quint64 rasterRevision() const;
    QColor effectiveMaskColor() const;
    QList<ImageMaskComponent> maskComponents() const;
    QList<ImageColorLayer> colorLayers() const;
    void setColorLayers(const QList<ImageColorLayer> &layers);
    int layerPosition(int layerIndex) const;
    bool hasRotation() const;
    QString xmlExportSummary() const;
    QByteArray sourcePayload() const;
    void setSourcePayload(const QByteArray &payload);
    // Растры, восстановленные из init/data, сохраняют границы цветовых масок.
    void restoreRasterComponents(const QList<ImageMaskComponent> &components, const QSize &size);
    void copyRasterSource(const ImageObject &source);
    QList<ImageMaskComponent> restoredRasterComponents() const { return m_restoredComponents; }
    QSize restoredRasterSize() const { return m_restoredSize; }
    void restoreRotationCoefficients(int sine, int cosine);
    QPoint rotationCoefficients() const;

    // Независимая копия параметров, ресурсов и состояния редактора.
    QSharedPointer<BaseObject> clone() const override;

    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override;
    QPolygonF selectionPolygon() const override;
    bool supportsRotationHandle() const override { return isRotationObject() && canResize(); }
    QTransform rotationFrame() const override;
    QPointF rotationCenter() const override;
    bool contains(const QPointF &point) const override;

    void moveBy(double dx, double dy) override;
    void resizeBy(int edgeFlags, double dx, double dy, bool proportional = false) override;
    void setRotation(double angle) override;
    bool setObjectProperty(const QString &name, const QString &value) override;

private:
    // RasterCache — один набор растров на объект; публичные параметры проверяются перед чтением кэша.
    struct RasterCache
    {
        double width = 0, height = 0;
        bool mirrorHorizontal = false, mirrorVertical = false, autoMaskColor = true;
        QColor maskColor;
        QString format;
        quint64 sourceRevision = 0;
        QList<ImageColorLayer> layers;
        bool valid = false, sourceReady = false, imageReady = false, componentsReady = false;
        QImage source, image;
        QList<ImageMaskComponent> components;
    };
    void ensureRasterCache() const;
    QImage buildSourceImage() const;
    QImage buildRenderedImage() const;
    QList<ImageMaskComponent> buildMaskComponents() const;
    quint64 m_sourceRevision = 0;
    mutable quint64 m_rasterRevision = 0;
    mutable RasterCache m_rasterCache;
    const Kind m_kind;
    void rebuildColorLayers();
    QImage m_sourceImage;
    QByteArray m_sourceBytes;
    QList<ImageColorLayer> m_colorLayers;
    QList<ImageMaskComponent> m_restoredComponents;
    QSize m_restoredSize;
    QPoint m_restoredRotation;
    double m_restoredAngle = 0.0;
    bool m_hasRestoredRotation = false;
};
