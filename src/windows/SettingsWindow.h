/**
 * @file SettingsWindow.h
 * @brief Окно настроек приложения
 */

#pragma once

#include <QDialog>

class QComboBox;
class QPushButton;

/**
 * @class SettingsWindow
 * @brief Диалоговое окно настроек приложения
 */
class SettingsWindow : public QDialog
{
    Q_OBJECT
    
public:
    explicit SettingsWindow(QWidget *parent = nullptr);

private slots:
    // Обработчик смены темы
    void onThemeChanged(int index);
    
    // Применить настройки
    void onApplyClicked();

private:
    void createWidgets();
    void loadCurrentSettings();
    
    QComboBox *m_themeCombo;     // Выбор темы
    QPushButton *m_applyButton;  // Кнопка применить
    QPushButton *m_closeButton;  // Кнопка закрыть
};
