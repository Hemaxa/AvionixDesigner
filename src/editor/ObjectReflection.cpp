// ObjectReflection - отражение геометрии, масок и локальных центров без изменения схем ПЛИС.

#include "ObjectReflection.h"
#include "RectangleObject.h"
#include "DashedLineObject.h"
#include "RibbonScaleObject.h"
#include "ImageObject.h"
#include "StaticGroupObject.h"
#include "RotationObject.h"
#include "TextObject.h"
#include <QtMath>

bool supportsObjectReflection(const BaseObject *object)
{
    return object && !dynamic_cast<const TextObject *>(object)
        && (dynamic_cast<const RectangleObject *>(object) || dynamic_cast<const DashedLineObject *>(object)
            || dynamic_cast<const RibbonScaleObject *>(object) || dynamic_cast<const ImageObject *>(object)
            || dynamic_cast<const StaticGroupObject *>(object) || dynamic_cast<const RotationObject *>(object));
}

bool reflectObject(BaseObject *object, const SymmetryAxis &axis)
{
    if (!supportsObjectReflection(object)) return false;
    const bool vertical = axis.orientation == Qt::Vertical;
    const double twice = 2 * axis.position;
    if (auto *v = dynamic_cast<RectangleObject *>(object)) {
        if (vertical) v->x = twice - v->x - v->width;
        else v->y = twice - v->y - v->height;
    } else if (auto *v = dynamic_cast<DashedLineObject *>(object)) {
        // Порядок концов сохраняется: фаза штриха отсчитывается от того же конца.
        if (vertical) { v->x0 = twice - v->x0; v->x1 = twice - v->x1; }
        else { v->y0 = twice - v->y0; v->y1 = twice - v->y1; }
    } else if (auto *v = dynamic_cast<RibbonScaleObject *>(object)) {
        if (vertical) { const double left = v->left; v->left = twice - v->right; v->right = twice - left; }
        else {
            const double top = v->top; v->top = twice - v->bottom; v->bottom = twice - top;
            v->yStart = twice - v->yStart;
            if (v->yStart < 0 || v->yStart > 4095) {
                const double period = qMax(1, v->period);
                v->yStart = std::fmod(std::fmod(v->yStart, period) + period, period);
            }
        }
    } else if (auto *v = dynamic_cast<ImageObject *>(object)) {
        if (vertical) { v->x = twice - v->x - v->width; v->mirrorHorizontal = !v->mirrorHorizontal; }
        else { v->y = twice - v->y - v->height; v->mirrorVertical = !v->mirrorVertical; }
        const double angle = v->rotationDegrees;
        const QPoint coefficients = v->rotationCoefficients();
        v->restoreRotationCoefficients(-coefficients.x(), coefficients.y());
        v->rotationDegrees = -angle;
    } else if (auto *v = dynamic_cast<StaticGroupObject *>(object)) {
        for (auto &state : v->states) {
            if (vertical) state.x = qRound(twice - state.x - state.w);
            else state.y = qRound(twice - state.y - state.h);
        }
        for (auto &mask : v->maskImages) mask = mask.mirrored(vertical, !vertical);
    } else if (auto *v = dynamic_cast<RotationObject *>(object)) {
        auto layers = v->hardwareLayers();
        if (layers.isEmpty()) layers.append({v->left, v->top, v->right, v->bottom,
            v->xRot, v->yRot, v->sinVal, v->cosVal, v->color, v->enabled});
        for (auto &layer : layers) {
            if (vertical) {
                const double left = layer.left; layer.left = -layer.right; layer.right = -left;
                layer.xRot = twice - layer.xRot;
            } else {
                const double top = layer.top; layer.top = -layer.bottom; layer.bottom = -top;
                layer.yRot = twice - layer.yRot;
            }
            layer.sinVal = -layer.sinVal;
        }
        v->maskImage = v->maskImage.mirrored(vertical, !vertical);
        v->restoreHardwareLayers(layers);
        if (!layers.isEmpty()) {
            const auto &first = layers.first();
            v->left = first.left; v->right = first.right; v->top = first.top; v->bottom = first.bottom;
            v->xRot = first.xRot; v->yRot = first.yRot; v->sinVal = first.sinVal; v->cosVal = first.cosVal;
        }
    }
    if (object->editorRotationPivot()) {
        const auto pivot = *object->editorRotationPivot();
        object->setEditorRotationPivot(QPointF(vertical ? 1 - pivot.x() : pivot.x(), vertical ? pivot.y() : 1 - pivot.y()));
    }
    emit object->changed();
    return true;
}
