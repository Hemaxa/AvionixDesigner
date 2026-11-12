#include "ObjectCloner.h"
#include "SymmetryCopyObject.h"

QList<QSharedPointer<BaseObject>> ObjectCloner::cloneScene(const QList<QSharedPointer<BaseObject>> &objects)
{
    QList<QSharedPointer<BaseObject>> clones;
    clones.reserve(objects.size());
    for (const auto &object : objects)
        clones.append(object ? object->clone() : QSharedPointer<BaseObject>{});

    // Несколько отражений одной оси должны разделять одну новую ось,
    // но не менять ось исходной сцены при Undo/Redo или дублировании слоя.
    QMap<const SymmetryAxis *, QSharedPointer<SymmetryAxis>> axes;
    for (qsizetype i = 0; i < objects.size(); ++i) {
        const auto *copy = dynamic_cast<SymmetryCopyObject *>(objects[i].data());
        if (!copy) continue;
        const auto sourceIndex = objects.indexOf(copy->source());
        if (sourceIndex < 0) continue;
        if (!axes.contains(copy->axis().data()))
            axes.insert(copy->axis().data(), QSharedPointer<SymmetryAxis>::create(*copy->axis()));
        clones[i] = copy->cloneWithLinks(clones[sourceIndex], axes.value(copy->axis().data()));
    }
    return clones;
}
