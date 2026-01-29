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
    static AppearanceManager* instance();
    
    // ===== Стили =====
    bool loadStyleSheet(const QString &filePath);
    void applyDarkTheme();
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
    void styleChanged();

private:
    AppearanceManager();
    
    QString m_currentStylePath;
    QMap<QString, QColor> m_colors;
    QFont m_monoFont;
    QFont m_uiFont;
};
