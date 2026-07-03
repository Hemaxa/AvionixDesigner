#include "SettingsWindow.h"
#include "AppearanceManager.h" 
#include "ProjectManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QListWidget>
#include <QStackedWidget>
#include <QSpinBox>
#include <QColorDialog>

SettingsWindow::SettingsWindow(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Настройки");
    setObjectName("SettingsWindow");
    setModal(true);
    setMinimumSize(550, 450);
    
    createWidgets();
    loadSettings();
}

void SettingsWindow::createWidgets()
{
    auto *mainLayout = new QVBoxLayout(this);
    auto *contentLayout = new QHBoxLayout();
    
    m_listWidget = new QListWidget(this);
    m_listWidget->setFixedWidth(150);
    m_listWidget->addItem("Настройки");
    m_listWidget->addItem("Горячие клавиши");
    
    m_stackedWidget = new QStackedWidget(this);
    
    // Вкладка "Настройки"
    auto *settingsWidget = new QWidget(this);
    auto *settingsLayout = new QVBoxLayout(settingsWidget);
    
    auto *formLayout = new QFormLayout();
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    
    auto *themeLabel = new QLabel("Тема: Avionix Designer", this);
    themeLabel->setObjectName("SettingsInfoLabel");
    formLayout->addRow(themeLabel);
    
    m_autoLoadCheck = new QCheckBox("Загружать последний проект", this);
    formLayout->addRow(m_autoLoadCheck);
    
    m_gridStepSpin = new QSpinBox(this);
    m_gridStepSpin->setRange(1, 100);
    m_gridStepSpin->setFixedWidth(100);
    formLayout->addRow("Шаг сетки:", m_gridStepSpin);
    
    m_gridColorButton = new QPushButton(this);
    m_gridColorButton->setFixedSize(24, 24);
    connect(m_gridColorButton, &QPushButton::clicked, this, &SettingsWindow::onGridColorClicked);
    formLayout->addRow("Цвет сетки:", m_gridColorButton);
    
    settingsLayout->addLayout(formLayout);
    settingsLayout->addStretch();
    m_stackedWidget->addWidget(settingsWidget);
    
    // Вкладка "Горячие клавиши"
    auto *hotkeysWidget = new QWidget(this);
    auto *hotkeysLayout = new QFormLayout(hotkeysWidget);
    hotkeysLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    hotkeysLayout->addRow(new QLabel("<b>Файл:</b>", this));
    hotkeysLayout->addRow("Создать проект:", new QLabel("Ctrl+N", this));
    hotkeysLayout->addRow("Открыть проект:", new QLabel("Ctrl+O", this));
    hotkeysLayout->addRow("Сохранить проект:", new QLabel("Ctrl+S", this));
    hotkeysLayout->addRow("Сохранить как:", new QLabel("Ctrl+Shift+S", this));
    hotkeysLayout->addRow("Экспорт кадра:", new QLabel("Ctrl+E", this));
    hotkeysLayout->addRow("Добавить изображение:", new QLabel("Ctrl+I", this));
    
    hotkeysLayout->addRow(new QLabel("<br><b>Редактирование:</b>", this));
    hotkeysLayout->addRow("Удалить объект:", new QLabel("Delete / Backspace", this));
    hotkeysLayout->addRow("Сбросить масштаб:", new QLabel("Ctrl+0", this));
    hotkeysLayout->addRow("Привязка к сетке:", new QLabel("Shift+G", this));
    hotkeysLayout->addRow("Привязка к экрану:", new QLabel("Shift+C", this));
    hotkeysLayout->addRow("Привязка к объектам:", new QLabel("Shift+O", this));
    
    hotkeysLayout->addRow(new QLabel("<br><b>Добавление объектов:</b>", this));
    hotkeysLayout->addRow("Добавить объект по типу:", new QLabel("Alt+1, Alt+2, ...", this));
    
    m_stackedWidget->addWidget(hotkeysWidget);
    
    connect(m_listWidget, &QListWidget::currentRowChanged, m_stackedWidget, &QStackedWidget::setCurrentIndex);
    m_listWidget->setCurrentRow(0);
    
    contentLayout->addWidget(m_listWidget);
    contentLayout->addWidget(m_stackedWidget);
    
    mainLayout->addLayout(contentLayout);
    
    auto *bottomLayout = new QHBoxLayout();
    QPushButton *resetButton = new QPushButton("Сбросить все настройки", this);
    resetButton->setObjectName("ResetButton");
    bottomLayout->addWidget(resetButton);
    bottomLayout->addStretch();
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    bottomLayout->addWidget(buttonBox);
    mainLayout->addLayout(bottomLayout);
    
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsWindow::saveSettings);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsWindow::close);
    connect(resetButton, &QPushButton::clicked, this, &SettingsWindow::resetAllSettings);
}

void SettingsWindow::onGridColorClicked()
{
    const QColor selectedColor = QColorDialog::getColor(m_currentGridColor, this, "Цвет сетки", QColorDialog::ShowAlphaChannel);
    if (selectedColor.isValid()) {
        m_currentGridColor = selectedColor;
        m_gridColorButton->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 4px;").arg(m_currentGridColor.name(QColor::HexArgb)));
    }
}

void SettingsWindow::loadSettings()
{
    QSettings settings("Avionix", "Designer");
    m_autoLoadCheck->setChecked(settings.value("autoLoad", false).toBool());
    
    m_gridStepSpin->setValue(settings.value("gridStep", 10).toInt());
    m_currentGridColor = QColor(settings.value("gridColor", "#3778b4c8").toString());
    m_gridColorButton->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 4px;").arg(m_currentGridColor.name(QColor::HexArgb)));
}

void SettingsWindow::saveSettings()
{
    QSettings settings("Avionix", "Designer");
    settings.setValue("autoLoad", m_autoLoadCheck->isChecked());
    settings.setValue("gridStep", m_gridStepSpin->value());
    settings.setValue("gridColor", m_currentGridColor.name(QColor::HexArgb));
    
    AppearanceManager::instance()->applyAvionixTheme();
    ProjectManager::instance()->reloadGlobalSettings();

    accept(); 
}

void SettingsWindow::resetAllSettings()
{
    QSettings settings("Avionix", "Designer");
    settings.clear();
    
    AppearanceManager::instance()->applyAvionixTheme();
    ProjectManager::instance()->reloadGlobalSettings();
    loadSettings();
    
    emit settingsReset();
}
