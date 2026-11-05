// AppearanceManager - менеджер внешнего вида приложения.

#pragma once

#include <QObject>

// AppearanceManager - менеджер внешнего вида приложения.
class AppearanceManager : public QObject
{
    Q_OBJECT

public:
    // получение единственного экземпляра
    static AppearanceManager* instance();

    // загружает стили из файла
    bool loadStyleSheet(const QString &filePath);

    // применяет тему Avionix Designer
    void applyAvionixTheme();

signals:
    void styleChanged();  //сигнал смены стиля

private:
    AppearanceManager();
};
