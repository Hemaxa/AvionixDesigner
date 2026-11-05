// ApplicationSettings - единое хранилище настроек и отдельный профиль для проверок.

#pragma once

#include <QSettings>

namespace ApplicationSettings {
inline QSettings create()
{
    // По умолчанию сохраняется штатное хранилище приложения. Явный путь нужен
    // автоматическим проверкам и экспорту без окна, чтобы не читать профиль пользователя.
    const QString path = qEnvironmentVariable("AVIONIX_SETTINGS_FILE");
    if (path.isEmpty()) return QSettings(QStringLiteral("Avionix"), QStringLiteral("Designer"));
    return QSettings(path, QSettings::IniFormat);
}

// Возможности целевой ПЛИС. Значения общие для всех проектов; адрес маски
// в текущем протоколе имеет 16 бит независимо от настроенной ёмкости банка.
inline int staticGroupPixels()
{
    return qBound(1, create().value("fpga/staticGroupPixels", 65536).toInt(), 65536);
}

inline int staticGroupElements()
{
    return qBound(1, create().value("fpga/staticGroupElements", 8).toInt(), 65536);
}
}
