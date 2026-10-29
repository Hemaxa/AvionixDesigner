// Русский язык приложения и стандартных диалогов Qt на любой ОС.
#pragma once
#include <QApplication>
#include <QLocale>
#include <QTranslator>

inline bool installRussianTranslation(QApplication &app)
{
    QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));
    auto *translator = new QTranslator(&app);
    if (!translator->load(QStringLiteral(":/translations/qtbase_ru.qm"))) {
        delete translator;
        return false;
    }
    return app.installTranslator(translator);
}
