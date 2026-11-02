// FpgaResourceLimits - проверка ёмкости памяти и количества параметров растровых объектов.

#include "fpga/compiler/FpgaResourceLimits.h"

#include "ImageObject.h"
#include "RotationObject.h"
#include "StaticGroupObject.h"
#include "TextObject.h"
#include "RasterGeometry.h"

#include <QtMath>

namespace FpgaResourceLimits {
Report inspect(const BaseObject &object)
{
    Report report;
    if (dynamic_cast<const TextObject *>(&object))
        return report;

    if (const auto *image = dynamic_cast<const ImageObject *>(&object)) {
        if (image->isRotationObject()) {
            const auto raster = image->renderedSourceImage();
            const auto bounds = rasterAlphaBounds(raster);
            const auto size = bounds.isEmpty() ? raster.size() : bounds.size();
            const qint64 columns = (qMax(1, size.width()) + 7) / 8;
            const qint64 rows = (qMax(1, size.height()) + 7) / 8;
            if (columns * rows > rotationTiles)
                report.warnings.append(QStringLiteral("Память вращения: %1/%2 тайлов. При экспорте растр будет уменьшен; детализация изменится.")
                    .arg(columns * rows).arg(rotationTiles));
            if (columns > 255)
                report.warnings.append(QStringLiteral("Ширина растра требует %1 столбцов тайлов; поле sq допускает 255. При экспорте растр будет уменьшен.").arg(columns));
        } else {
            const auto components = image->maskComponents();
            qint64 pixels = 0;
            bool largeDimensions = false;
            for (const auto &component : components) {
                pixels += qint64(component.bounds.width()) * component.bounds.height();
                largeDimensions |= component.bounds.width() > 4095 || component.bounds.height() > 4095;
            }
            if (components.isEmpty())
                pixels = qint64(qMax(1, qRound(image->width))) * qMax(1, qRound(image->height));
            if (pixels > staticPixels)
                report.warnings.append(QStringLiteral("Память статической группы: %1/%2 ячеек. При экспорте растр будет уменьшен; детализация изменится.")
                    .arg(pixels).arg(staticPixels));
            if (largeDimensions)
                report.warnings.append(QStringLiteral("Габарит маски превышает 4095 пикселей. При экспорте растр будет уменьшен."));
            if (components.size() > staticParameters)
                report.errors.append(QStringLiteral("Статической группе требуется %1 init; допускается не более %2. Уменьшите число цветовых слоёв.")
                    .arg(components.size()).arg(staticParameters));
        }
    } else if (const auto *group = dynamic_cast<const StaticGroupObject *>(&object)) {
        qint64 end = 0;
        for (const auto &state : group->states) {
            end = qMax(end, qint64(state.addr) + qint64(state.w) * state.h);
            if (state.w < 0 || state.h < 0 || state.w > 4095 || state.h > 4095 || state.addr < 0 || state.addr >= staticPixels
                || state.x < 0 || state.y < 0 || state.x > 4095 || state.y > 4095)
                report.errors.append(QStringLiteral("Габариты или адрес статической маски выходят за диапазон полей XML."));
        }
        if (end > staticPixels)
            report.errors.append(QStringLiteral("Память статической группы: %1/%2 ячеек. Экспорт невозможен: уменьшите растры или разделите группу.")
                .arg(end).arg(staticPixels));
        if (group->states.size() > staticParameters)
            report.errors.append(QStringLiteral("В статической группе %1 init; допускается не более %2.")
                .arg(group->states.size()).arg(staticParameters));
    } else if (const auto *rotation = dynamic_cast<const RotationObject *>(&object)) {
        const qint64 width = rotation->maskImage.isNull() ? qMax(1, qRound(rotation->right - rotation->left)) : rotation->maskImage.width();
        const qint64 height = rotation->maskImage.isNull() ? qMax(1, qRound(rotation->bottom - rotation->top)) : rotation->maskImage.height();
        const qint64 tiles = ((width + 7) / 8) * ((height + 7) / 8);
        if (tiles > rotationTiles)
            report.errors.append(QStringLiteral("Память вращения: %1/%2 тайлов. Экспорт невозможен: уменьшите растр.")
                .arg(tiles).arg(rotationTiles));
        if ((width + 7) / 8 > 255)
            report.errors.append(QStringLiteral("Ширина растра требует более 255 столбцов тайлов и не помещается в поле sq."));
    }
    report.errors.removeDuplicates();
    return report;
}
}
