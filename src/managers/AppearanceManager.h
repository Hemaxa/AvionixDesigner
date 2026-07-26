//AppearanceManager - менеджер внешнего вида приложения

#pragma once

#include <QObject>

class AppearanceManager : public QObject
{
    Q_OBJECT
    
public:
    //получение единственного экземпляра
    static AppearanceManager* instance();
    
    //загружает стили из файла
    bool loadStyleSheet(const QString &filePath);
    
    //применяет тему Avionix Designer
    void applyAvionixTheme();
    
    //возвращает путь к текущей теме
    QString getCurrentStylePath() const;

signals:
    void styleChanged();  //сигнал смены стиля

private:
    AppearanceManager();
    
    QString m_currentStylePath; //путь к текущему файлу стилей
};
