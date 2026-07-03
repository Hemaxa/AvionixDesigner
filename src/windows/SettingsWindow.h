//SettingsWindow - окно настроек приложения

#pragma once

#include <QDialog>
#include <QColor>

class QCheckBox;
class QListWidget;
class QStackedWidget;
class QSpinBox;
class QPushButton;

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
    
    void onGridColorClicked();

private:
    //метод создания виджетов и панелей
    void createWidgets();

    //метод загрузки настроек
    void loadSettings();

    //элементы интерфейса
    QListWidget *m_listWidget;
    QStackedWidget *m_stackedWidget;
    QCheckBox *m_autoLoadCheck;
    
    QSpinBox *m_gridStepSpin;
    QPushButton *m_gridColorButton;
    QColor m_currentGridColor;
};
