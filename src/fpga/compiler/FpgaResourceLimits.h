// FpgaResourceLimits - проверка ёмкости памяти и количества параметров растровых объектов.

#pragma once

#include <QStringList>
#include "ApplicationSettings.h"

class BaseObject;

namespace FpgaResourceLimits {
inline int staticParameters() { return ApplicationSettings::staticGroupElements(); }
inline int staticPixels() { return ApplicationSettings::staticGroupPixels(); }
constexpr int rotationTiles = 2048;

// Report - ошибки экспорта и предупреждения об уменьшении растров.
struct Report
{
    QStringList errors;   // Нарушения, блокирующие аппаратный экспорт.
    QStringList warnings; // Уменьшение редактируемого растра существующим экспортёром.
    QStringList messages() const { return errors + warnings; }
};

// Проверяет статические/вращающиеся растры; готовые маски текста проверяются также компилятором сцены.
Report inspect(const BaseObject &object);
}
