#include "SettingsWindow.h"
#include "AppearanceManager.h"
#include "ProjectManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QIcon>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QListWidget>
#include <QStackedWidget>
#include <QSpinBox>
#include <QColorDialog>
#include <QPainter>
#include <QPixmap>

SettingsWindow::SettingsWindow(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Настройки");
    setObjectName("SettingsWindow");
    setModal(true);
    setMinimumSize(760, 560);
    resize(820, 600);

    createWidgets();
    loadSettings();
}

void SettingsWindow::createWidgets()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 22, 24, 20);
    mainLayout->setSpacing(18);
    auto *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(18);

    m_listWidget = new QListWidget(this);
    m_listWidget->setFixedWidth(220);
    m_listWidget->addItem("Настройки");
    m_listWidget->addItem("Горячие клавиши");

    m_stackedWidget = new QStackedWidget(this);

    // Вкладка "Настройки"
    auto *settingsWidget = new QWidget(this);
    auto *settingsLayout = new QVBoxLayout(settingsWidget);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(18);

    auto *generalGroup = new QGroupBox(QStringLiteral("Общие"), this);
    auto *generalLayout = new QFormLayout(generalGroup);
    generalLayout->setContentsMargins(18, 20, 18, 18);
    generalLayout->setHorizontalSpacing(18);
    generalLayout->setVerticalSpacing(14);
    generalLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    generalLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_autoLoadCheck = new QCheckBox("Загружать последний проект", this);
    generalLayout->addRow(m_autoLoadCheck);
    settingsLayout->addWidget(generalGroup);

    auto *snapGroup = new QGroupBox(QStringLiteral("Сетка и привязки"), this);
    auto *formLayout = new QFormLayout(snapGroup);
    formLayout->setContentsMargins(18, 20, 18, 18);
    formLayout->setHorizontalSpacing(18);
    formLayout->setVerticalSpacing(14);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_gridStepSpin = new QSpinBox(this);
    m_gridStepSpin->setRange(1, 100);
    m_gridStepSpin->setMinimumWidth(160);
    formLayout->addRow("Шаг сетки:", m_gridStepSpin);

    m_gridColorButton = new QPushButton(this);
    m_gridColorButton->setObjectName("ColorSwatchButton");
    m_gridColorButton->setFixedSize(44, 34);
    connect(m_gridColorButton, &QPushButton::clicked, this, [this]() {
        chooseColor(&m_currentGridColor, m_gridColorButton, QStringLiteral("Цвет сетки"));
    });
    formLayout->addRow("Цвет сетки:", m_gridColorButton);

    m_snapCanvasColorButton = new QPushButton(this);
    m_snapCanvasColorButton->setObjectName("ColorSwatchButton");
    m_snapCanvasColorButton->setFixedSize(44, 34);
    connect(m_snapCanvasColorButton, &QPushButton::clicked, this, [this]() {
        chooseColor(&m_currentSnapCanvasColor, m_snapCanvasColorButton, QStringLiteral("Цвет привязки к границам"));
    });
    formLayout->addRow("Линии границ:", m_snapCanvasColorButton);

    m_snapGridColorButton = new QPushButton(this);
    m_snapGridColorButton->setObjectName("ColorSwatchButton");
    m_snapGridColorButton->setFixedSize(44, 34);
    connect(m_snapGridColorButton, &QPushButton::clicked, this, [this]() {
        chooseColor(&m_currentSnapGridColor, m_snapGridColorButton, QStringLiteral("Цвет привязки к сетке"));
    });
    formLayout->addRow("Линии сетки:", m_snapGridColorButton);

    m_snapObjectColorButton = new QPushButton(this);
    m_snapObjectColorButton->setObjectName("ColorSwatchButton");
    m_snapObjectColorButton->setFixedSize(44, 34);
    connect(m_snapObjectColorButton, &QPushButton::clicked, this, [this]() {
        chooseColor(&m_currentSnapObjectColor, m_snapObjectColorButton, QStringLiteral("Цвет привязки к объектам"));
    });
    formLayout->addRow("Линии объектов:", m_snapObjectColorButton);

    settingsLayout->addWidget(snapGroup);
    settingsLayout->addStretch();
    m_stackedWidget->addWidget(settingsWidget);

    // Вкладка "Горячие клавиши"
    auto *hotkeysWidget = new QWidget(this);
    auto *hotkeysLayout = new QFormLayout(hotkeysWidget);
    hotkeysLayout->setContentsMargins(0, 0, 0, 0);
    hotkeysLayout->setHorizontalSpacing(22);
    hotkeysLayout->setVerticalSpacing(12);
    hotkeysLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    hotkeysLayout->addRow(new QLabel("<b>Файл:</b>", this));
    hotkeysLayout->addRow("Создать проект:", new QLabel("Ctrl+N", this));
    hotkeysLayout->addRow("Открыть проект:", new QLabel("Ctrl+O", this));
    hotkeysLayout->addRow("Сохранить проект:", new QLabel("Ctrl+S", this));
    hotkeysLayout->addRow("Сохранить как:", new QLabel("Ctrl+Shift+S", this));
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
    bottomLayout->setSpacing(12);
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

void SettingsWindow::chooseColor(QColor *targetColor, QPushButton *button, const QString &title)
{
    if (!targetColor || !button)
        return;

    const QColor selectedColor = QColorDialog::getColor(*targetColor, this, title, QColorDialog::ShowAlphaChannel);
    if (selectedColor.isValid()) {
        *targetColor = selectedColor;
        updateColorButtonIcon(button, selectedColor);
    }
}

void SettingsWindow::updateColorButtonIcon(QPushButton *button, const QColor &color)
{
    if (!button)
        return;

    QPixmap pixmap(30, 22);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(QPen(QColor(230, 245, 255, 170), 1));
    painter.drawRoundedRect(QRectF(1, 1, 28, 20), 4, 4);
    painter.end();
    button->setIcon(QIcon(pixmap));
}

void SettingsWindow::loadSettings()
{
    QSettings settings("Avionix", "Designer");
    m_autoLoadCheck->setChecked(settings.value("autoLoad", false).toBool());

    m_gridStepSpin->setValue(settings.value("gridStep", 10).toInt());
    m_currentGridColor = QColor(settings.value("gridColor", "#3778b4c8").toString());
    m_currentSnapCanvasColor = QColor(settings.value("snapCanvasGuideColor", "#d2ff5c7a").toString());
    m_currentSnapGridColor = QColor(settings.value("snapGridGuideColor", "#d256d3ff").toString());
    m_currentSnapObjectColor = QColor(settings.value("snapObjectGuideColor", "#dcffca58").toString());
    updateColorButtonIcon(m_gridColorButton, m_currentGridColor);
    updateColorButtonIcon(m_snapCanvasColorButton, m_currentSnapCanvasColor);
    updateColorButtonIcon(m_snapGridColorButton, m_currentSnapGridColor);
    updateColorButtonIcon(m_snapObjectColorButton, m_currentSnapObjectColor);
}

void SettingsWindow::saveSettings()
{
    QSettings settings("Avionix", "Designer");
    settings.setValue("autoLoad", m_autoLoadCheck->isChecked());
    settings.setValue("gridStep", m_gridStepSpin->value());
    settings.setValue("gridColor", m_currentGridColor.name(QColor::HexArgb));
    settings.setValue("snapCanvasGuideColor", m_currentSnapCanvasColor.name(QColor::HexArgb));
    settings.setValue("snapGridGuideColor", m_currentSnapGridColor.name(QColor::HexArgb));
    settings.setValue("snapObjectGuideColor", m_currentSnapObjectColor.name(QColor::HexArgb));

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
