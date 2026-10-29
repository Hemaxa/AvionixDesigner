// ApplicationSettings — единое хранилище настроек и отдельный профиль для проверок.
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
}
