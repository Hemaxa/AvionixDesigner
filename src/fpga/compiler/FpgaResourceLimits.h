// FpgaResourceLimits - проверка ёмкости памяти и количества параметров растровых объектов.

#pragma once

#include <QStringList>
#include "ApplicationSettings.h"

class BaseObject;

namespace FpgaResourceLimits {
inline int staticParameters() { return ApplicationSettings::staticGroupElements(); }
inline int staticPixels() { return ApplicationSettings::staticGroupPixels(); }
constexpr int rotationTiles = 2048;
// SYMBOLs использует bram[BD:0] с BD = 16384 в поставляемом RTL.
constexpr int fontPixels = 16385;

// Report - ошибки экспорта и предупреждения об уменьшении растров.
struct Report
{
    QStringList errors;   // Нарушения, блокирующие аппаратный экспорт.
    QStringList warnings; // Уменьшение редактируемого растра существующим экспортёром.
    QStringList messages() const { return errors + warnings; }
};

// Проверяет статические/вращающиеся растры. Атласы шрифтов проверяет компилятор сцены.
Report inspect(const BaseObject &object);
}
