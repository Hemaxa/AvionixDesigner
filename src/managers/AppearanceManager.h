/**
 * @file AppearanceManager.h
 * @brief Менеджер внешнего вида приложения
 */

#pragma once

#include <QObject>
#include <QFont>
#include <QColor>
#include <QMap>

/**
 * @class AppearanceManager
 * @brief Менеджер тем и стилей приложения (синглтон)
 */
class AppearanceManager : public QObject
{
    Q_OBJECT
    
public:
    // Получение единственного экземпляра
    static AppearanceManager* instance();
    
    // ===== Стили =====
    
    // Загружает стили из файла
    bool loadStyleSheet(const QString &filePath);
    
    // Применяет тёмную тему
    void applyDarkTheme();
    
    // Применяет светлую тему
    void applyLightTheme();
    
    // Применяет тему Avionix Designer
    void applyAvionixTheme();
    
    // Возвращает путь к текущей теме
    QString getCurrentStylePath() const;
    
    // ===== Геттеры/сеттеры цветов темы =====
    QColor getColor(const QString &name) const;
    void setColor(const QString &name, const QColor &color);
    
    // ===== Геттеры/сеттеры шрифтов =====
    QFont getMonoFont() const;
    QFont getUiFont() const;
    void setMonoFont(const QFont &font);
    void setUiFont(const QFont &font);

signals:
    void styleChanged();  // Сигнал смены стиля

private:
    AppearanceManager();
    
    QString m_currentStylePath;       // Путь к текущему файлу стилей
    QMap<QString, QColor> m_colors;   // Палитра цветов
    QFont m_monoFont;                 // Моноширинный шрифт
    QFont m_uiFont;                   // Шрифт интерфейса
};
