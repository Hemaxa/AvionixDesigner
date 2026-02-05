/**
 * @file AppearanceManager.cpp
 * @brief Реализация менеджера внешнего вида
 */

#include "AppearanceManager.h"
#include <QApplication>
#include <QFile>

AppearanceManager::AppearanceManager()
{
    // Инициализация шрифтов
    m_monoFont = QFont("Menlo", 11);
    m_monoFont.setStyleHint(QFont::Monospace);
    
    m_uiFont = QFont("SF Pro", 13);
    m_uiFont.setStyleHint(QFont::SansSerif);
    
    // Инициализация палитры цветов (тёмная тема по умолчанию)
    m_colors["background"] = QColor(0x2d, 0x2d, 0x2d);
    m_colors["foreground"] = QColor(0xe0, 0xe0, 0xe0);
    m_colors["accent"] = QColor(0x4a, 0x90, 0xd9);
    m_colors["selection"] = QColor(0x4a, 0x90, 0xd9);
    m_colors["error"] = QColor(0xf4, 0x43, 0x36);
    m_colors["warning"] = QColor(0xff, 0x98, 0x00);
    m_colors["success"] = QColor(0x4c, 0xaf, 0x50);
    m_colors["border"] = QColor(0x1a, 0x1a, 0x1a);
    m_colors["hover"] = QColor(0x3a, 0x3a, 0x3a);
}

AppearanceManager* AppearanceManager::instance()
{
    static AppearanceManager s_instance;
    return &s_instance;
}

bool AppearanceManager::loadStyleSheet(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("AppearanceManager: Не удалось загрузить стиль: %s", 
                 qPrintable(filePath));
        return false;
    }
    
    QString styleSheet = QString::fromUtf8(file.readAll());
    file.close();
    
    qApp->setStyleSheet(styleSheet);
    m_currentStylePath = filePath;
    
    emit styleChanged();
    return true;
}

void AppearanceManager::applyDarkTheme()
{
    // Загружаем тёмную тему из файла
    if (!loadStyleSheet("res/themes/DarkTheme.qss")) {
        qWarning("AppearanceManager: Не удалось загрузить DarkTheme.qss");
    }
    
    // Обновляем палитру цветов
    m_colors["background"] = QColor(0x2d, 0x2d, 0x2d);
    m_colors["foreground"] = QColor(0xe0, 0xe0, 0xe0);
}

void AppearanceManager::applyLightTheme()
{
    // Загружаем светлую тему из файла
    if (!loadStyleSheet("res/themes/LightTheme.qss")) {
        qWarning("AppearanceManager: Не удалось загрузить LightTheme.qss");
    }
    
    // Обновляем палитру цветов
    m_colors["background"] = QColor(0xf5, 0xf5, 0xf5);
    m_colors["foreground"] = QColor(0x2d, 0x2d, 0x2d);
}

QString AppearanceManager::getCurrentStylePath() const
{
    return m_currentStylePath;
}

QColor AppearanceManager::getColor(const QString &name) const
{
    return m_colors.value(name, Qt::black);
}

void AppearanceManager::setColor(const QString &name, const QColor &color)
{
    m_colors[name] = color;
}

QFont AppearanceManager::getMonoFont() const
{
    return m_monoFont;
}

QFont AppearanceManager::getUiFont() const
{
    return m_uiFont;
}

void AppearanceManager::setMonoFont(const QFont &font)
{
    m_monoFont = font;
}

void AppearanceManager::setUiFont(const QFont &font)
{
    m_uiFont = font;
}
