#pragma once

#include "ImageObject.h"

// ImageResourceBuilder — подготовка масок изображений под размеры полей,
// ёмкость STATICGROUP и тайлы ROTATIONOBJECT. Исходный объект не изменяется.
class ImageResourceBuilder
{
public:
    static QSizeF fitSizeToStaticGroupLimits(const QSizeF &size, const ParamSchema &schema);
    static QList<ImageMaskComponent> simplifyComponentsForStaticGroup(const ParamSchema &schema,
                                                                     const QList<ImageMaskComponent> &components);
    static ImageMaskComponent singleComponentForRotationObject(const ImageObject *image);
    static ImageMaskComponent simplifyComponentForRotationObject(const ImageMaskComponent &component);

private:
    static int maxUnsignedFieldValue(const ParamSchema &schema, const QString &fieldName, int fallback);
    static int staticGroupAddressablePixels(const ParamSchema &schema);
    static qint64 staticGroupPixelCost(const QList<ImageMaskComponent> &components);
    static QList<ImageMaskComponent> scaledStaticGroupComponents(const QList<ImageMaskComponent> &components, double scale);
    static int rotationTileCost(const QSize &size);
};
