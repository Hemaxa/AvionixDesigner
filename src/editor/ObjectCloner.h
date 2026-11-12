#pragma once

#include "BaseObject.h"

// ObjectCloner — независимая копия сцены для истории и дублирования слоёв.
// Объекты копируются полиморфно, связи симметрии перенаправляются внутри копии.
class ObjectCloner
{
public:
    static QList<QSharedPointer<BaseObject>> cloneScene(const QList<QSharedPointer<BaseObject>> &objects);
};
