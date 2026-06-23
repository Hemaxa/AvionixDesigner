#include "SettingsWindow.h"
#include "AppearanceManager.h" 

#include <QVBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QSettings>

SettingsWindow::SettingsWindow(QWidget *parent) : QDialog(parent)
{
    //заголовок окна и имя объекта
    setWindowTitle("Настройки");
    setObjectName("SettingsWindow");
    
    //установка модального поведения
    setModal(true);
    
    //фиксированный размер
    setMinimumWidth(300);
    
    //служебные методы создания
    createWidgets();
    
    //загрузка настроек
    loadSettings();
}

void SettingsWindow::createWidgets()
{
    //вертикальный шаблон
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    //форма настроек
    QFormLayout *formLayout = new QFormLayout();
    
    auto *themeLabel = new QLabel("Тема: Avionix Designer", this);
    themeLabel->setObjectName("SettingsInfoLabel");
    formLayout->addRow("", themeLabel);
    
    //настройка автозагрузки (для примера)
    m_autoLoadCheck = new QCheckBox("Загружать последний проект", this);
    formLayout->addRow("", m_autoLoadCheck);
    
    mainLayout->addLayout(formLayout);
    
    mainLayout->addStretch();
    
    //кнопка сброса настроек
    QPushButton *resetButton = new QPushButton("Сбросить все настройки", this);
    resetButton->setObjectName("ResetButton");
    mainLayout->addWidget(resetButton);
    
    mainLayout->addSpacing(8);
    
    //кнопки OK/Cancel
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    mainLayout->addWidget(buttonBox);
    
    //подключение сигналов
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsWindow::saveSettings);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsWindow::close);
    connect(resetButton, &QPushButton::clicked, this, &SettingsWindow::resetAllSettings);
}

//метод загрузки настроек при открытии окна
void SettingsWindow::loadSettings()
{
    //идентификаторы настроек
    QSettings settings("Avionix", "Designer");
    
    //читаем чекбокса автозагрузки файла и его правильное отображение
    bool autoLoad = settings.value("autoLoad", false).toBool();
    m_autoLoadCheck->setChecked(autoLoad);
}

//метод сохранения настроек (вызывается по кнопке ОК)
void SettingsWindow::saveSettings()
{
    //идентификаторы настроек
    QSettings settings("Avionix", "Designer");
    
    //записываем состояние чекбокса
    settings.setValue("autoLoad", m_autoLoadCheck->isChecked());
    
    AppearanceManager::instance()->applyAvionixTheme();

    accept(); 
}

void SettingsWindow::resetAllSettings()
{
    //очищаем все настройки
    QSettings settings("Avionix", "Designer");
    settings.clear();
    
    //применяем единую тему приложения
    AppearanceManager::instance()->applyAvionixTheme();
    
    //перезагружаем UI с дефолтными значениями
    loadSettings();
    
    //отправляем сигнал для сброса layout в MainWindow
    emit settingsReset();
}
