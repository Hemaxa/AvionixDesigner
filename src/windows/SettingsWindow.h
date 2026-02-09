//SettingsWindow - окно настроек приложения

#pragma once

#include <QDialog>

class QComboBox;
class QCheckBox;

class SettingsWindow : public QDialog
{
    Q_OBJECT
    
public:
    //конструктор с запретом на неявное преобразование типов
    explicit SettingsWindow(QWidget *parent = nullptr);

private slots:
    // Сохранить настройки и применить изменения
    void saveSettings();

private:
    void createWidgets();
    void loadSettings();

    //элементы интерфейса
    QComboBox *m_themeCombo; //выпадающий список для выбора темы
    QCheckBox *m_autoLoadCheck; //чекбокс для автозагрузки последнего проекта
};