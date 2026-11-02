#pragma once

#include <QStringList>

class BaseObject;

namespace FpgaResourceLimits {
constexpr int staticParameters = 8;
constexpr int staticPixels = 65536;
constexpr int rotationTiles = 2048;

struct Report
{
    QStringList errors;
    QStringList warnings;
    QStringList messages() const { return errors + warnings; }
};

Report inspect(const BaseObject &object);
}
