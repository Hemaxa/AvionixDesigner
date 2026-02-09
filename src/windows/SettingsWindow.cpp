#include "SettingsWindow.h"
#include "AppearanceManager.h" 

#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
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
    
    //настройка темы
    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem("Тёмная");
    m_themeCombo->addItem("Светлая");
    m_themeCombo->addItem("Avionix Designer");
    
    //добавляем строку в форму: Текст "Тема интерфейса:" и сам комбобокс
    formLayout->addRow("Тема интерфейса:", m_themeCombo);
    
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
    
    //чтение настройки темы и установка ее активной в поле
    int themeIndex = settings.value("theme", 0).toInt();
    m_themeCombo->setCurrentIndex(themeIndex);
    
    //читаем чекбокса автозагрузки файла и его правильное отображение
    bool autoLoad = settings.value("autoLoad", false).toBool();
    m_autoLoadCheck->setChecked(autoLoad);
}

//метод сохранения настроек (вызывается по кнопке ОК)
void SettingsWindow::saveSettings()
{
    //идентификаторы настроек
    QSettings settings("Avionix", "Designer");
    
    //записываем текущий выбранный индекс из комбобокса
    settings.setValue("theme", m_themeCombo->currentIndex());
    
    //записываем состояние чекбокса
    settings.setValue("autoLoad", m_autoLoadCheck->isChecked());
    
    //получаем менеджер внешнего вида
    auto am = AppearanceManager::instance();
    
    //смотрим, что выбрал пользователь
    switch (m_themeCombo->currentIndex()) {
    case 0:
        am->applyDarkTheme();
        break;
    case 1:
        am->applyLightTheme();
        break;
    case 2:
        am->applyAvionixTheme();
        break;
    }

    accept(); 
}

void SettingsWindow::resetAllSettings()
{
    //очищаем все настройки
    QSettings settings("Avionix", "Designer");
    settings.clear();
    
    //применяем тему по умолчанию
    AppearanceManager::instance()->applyDarkTheme();
    
    //перезагружаем UI с дефолтными значениями
    loadSettings();
    
    //отправляем сигнал для сброса layout в MainWindow
    emit settingsReset();
}
