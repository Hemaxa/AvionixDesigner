/**
 * @file SettingsWindow.cpp
 * @brief Реализация окна настроек
 */

#include "SettingsWindow.h"
#include "../managers/AppearanceManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>

SettingsWindow::SettingsWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Настройки");
    setObjectName("SettingsWindow");
    setMinimumSize(350, 200);
    setModal(true);
    
    createWidgets();
    loadCurrentSettings();
}

void SettingsWindow::createWidgets()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    
    // Группа настроек внешнего вида
    QGroupBox *appearanceGroup = new QGroupBox("Внешний вид", this);
    appearanceGroup->setObjectName("AppearanceGroup");
    
    QFormLayout *formLayout = new QFormLayout(appearanceGroup);
    formLayout->setSpacing(8);
    
    // Выбор темы
    m_themeCombo = new QComboBox(this);
    m_themeCombo->setObjectName("ThemeComboBox");
    m_themeCombo->addItem("Тёмная", "dark");
    m_themeCombo->addItem("Светлая", "light");
    formLayout->addRow("Тема:", m_themeCombo);
    
    mainLayout->addWidget(appearanceGroup);
    mainLayout->addStretch();
    
    // Кнопки управления
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_applyButton = new QPushButton("Применить", this);
    m_applyButton->setObjectName("ApplyButton");
    connect(m_applyButton, &QPushButton::clicked, this, &SettingsWindow::onApplyClicked);
    buttonLayout->addWidget(m_applyButton);
    
    m_closeButton = new QPushButton("Закрыть", this);
    m_closeButton->setObjectName("CloseButton");
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::close);
    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(buttonLayout);
}

void SettingsWindow::loadCurrentSettings()
{
    // Определяем текущую тему по пути файла
    QString currentPath = AppearanceManager::instance()->getCurrentStylePath();
    if (currentPath.contains("Light")) {
        m_themeCombo->setCurrentIndex(1);
    } else {
        m_themeCombo->setCurrentIndex(0);
    }
}

void SettingsWindow::onThemeChanged(int index)
{
    Q_UNUSED(index);
    // Тема применится при нажатии кнопки "Применить"
}

void SettingsWindow::onApplyClicked()
{
    // Получаем выбранную тему
    QString themeKey = m_themeCombo->currentData().toString();
    
    auto am = AppearanceManager::instance();
    if (themeKey == "light") {
        am->applyLightTheme();
    } else {
        am->applyDarkTheme();
    }
}
