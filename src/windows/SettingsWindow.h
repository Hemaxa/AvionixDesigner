//SettingsWindow - окно настроек приложения

#pragma once

#include <QDialog>

class QCheckBox;

class SettingsWindow : public QDialog
{
    Q_OBJECT
    
public:
    //конструктор с запретом на неявное преобразование типов
    explicit SettingsWindow(QWidget *parent = nullptr);

signals:
    //сигнал сброса всех настроек
    void settingsReset();

private slots:
    //сохранить настройки и применить изменения
    void saveSettings();
    
    //сброс всех настроек до стандартных
    void resetAllSettings();

private:
    //метод создания виджетов и панелей
    void createWidgets();

    //метод загрузки настроек
    void loadSettings();

    //элементы интерфейса
    QCheckBox *m_autoLoadCheck;
};
