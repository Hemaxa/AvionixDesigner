//AppearanceManager - менеджер внешнего вида приложения

#pragma once

#include <QObject>
#include <QFont>
#include <QColor>
#include <QMap>

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
    
    //геттеры/сеттеры цветов темы
    QColor getColor(const QString &name) const;
    void setColor(const QString &name, const QColor &color);
    
    //геттеры/сеттеры шрифтов темы
    QFont getMonoFont() const;
    QFont getUiFont() const;
    void setMonoFont(const QFont &font);
    void setUiFont(const QFont &font);

signals:
    void styleChanged();  //сигнал смены стиля

private:
    AppearanceManager();
    
    QString m_currentStylePath; //путь к текущему файлу стилей
    QMap<QString, QColor> m_colors; //палитра цветов
    QFont m_monoFont; //мноширинный шрифт
    QFont m_uiFont; //шрифт интерфейса
};
