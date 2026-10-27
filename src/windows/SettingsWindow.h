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
    
private:
    //метод создания виджетов и панелей
    void createWidgets();

    //метод загрузки настроек
    void loadSettings();
    void chooseColor(QColor *targetColor, QPushButton *button, const QString &title);
    void updateColorButtonIcon(QPushButton *button, const QColor &color);

    //элементы интерфейса
    QListWidget *m_listWidget;
    QStackedWidget *m_stackedWidget;
    QCheckBox *m_autoLoadCheck;
    
    QSpinBox *m_gridStepSpin;
    QPushButton *m_gridColorButton;
    QPushButton *m_snapCanvasColorButton;
    QPushButton *m_snapGridColorButton;
    QPushButton *m_snapObjectColorButton;
    QColor m_currentGridColor;
    QColor m_currentSnapCanvasColor;
    QColor m_currentSnapGridColor;
    QColor m_currentSnapObjectColor;
};
