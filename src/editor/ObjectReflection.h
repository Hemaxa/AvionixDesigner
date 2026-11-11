// ObjectReflection - фиксация зеркального преобразования в обычном объекте редактора.

#pragma once

#include "SymmetryCopyObject.h"

bool supportsObjectReflection(const BaseObject *object);
bool reflectObject(BaseObject *object, const SymmetryAxis &axis);
