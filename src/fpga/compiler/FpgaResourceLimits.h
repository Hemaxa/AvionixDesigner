// FpgaResourceLimits - проверка ёмкости памяти и количества параметров растровых объектов.

#pragma once

#include <QStringList>

class BaseObject;

namespace FpgaResourceLimits {
constexpr int staticParameters = 8;
constexpr int staticPixels = 65536;
constexpr int rotationTiles = 2048;

// Report - ошибки экспорта и предупреждения об уменьшении растров.
struct Report
{
    QStringList errors;   // Нарушения, блокирующие аппаратный экспорт.
    QStringList warnings; // Уменьшение редактируемого растра существующим экспортёром.
    QStringList messages() const { return errors + warnings; }
};

// Проверяет статические/вращающиеся растры; лимиты алфавитов проверяет компилятор сцены.
Report inspect(const BaseObject &object);
}
