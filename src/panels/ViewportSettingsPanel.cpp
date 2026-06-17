#include "ViewportSettingsPanel.h"

#include "ProjectManager.h"

#include <QColorDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ViewportSettingsPanel::ViewportSettingsPanel(QWidget *parent) : BasePanel(parent)
{
    setPanelName("ViewportSettingsPanel");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto *card = new QFrame(this);
    card->setObjectName("SettingsCard");

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(10, 10, 10, 10);
    cardLayout->setSpacing(8);

    auto *sizeRow = new QHBoxLayout();
    sizeRow->setSpacing(6);

    auto *sizeCaption = new QLabel("Размер", card);
    sizeCaption->setObjectName("SettingsFieldLabel");
    sizeRow->addWidget(sizeCaption);

    m_widthSpin = new QSpinBox(card);
    m_widthSpin->setRange(1, 8192);
    m_widthSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_widthSpin->setMinimumWidth(72);
    sizeRow->addWidget(m_widthSpin);

    auto *multiplyLabel = new QLabel("x", card);
    multiplyLabel->setObjectName("SettingsFieldLabel");
    sizeRow->addWidget(multiplyLabel);

    m_heightSpin = new QSpinBox(card);
    m_heightSpin->setRange(1, 8192);
    m_heightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_heightSpin->setMinimumWidth(72);
    sizeRow->addWidget(m_heightSpin);
    sizeRow->addStretch();

    cardLayout->addLayout(sizeRow);

    m_sizeLabel = new QLabel(QStringLiteral("—"), card);
    m_sizeLabel->setObjectName("SettingsSizeLabel");
    cardLayout->addWidget(m_sizeLabel);

    auto *bgRow = new QHBoxLayout();
    bgRow->setSpacing(6);

    auto *bgCaption = new QLabel("Фон", card);
    bgCaption->setObjectName("SettingsFieldLabel");
    bgRow->addWidget(bgCaption);

    m_bgColorButton = new QPushButton(card);
    m_bgColorButton->setObjectName("SettingsColorButton");
    m_bgColorButton->setFixedSize(28, 28);
    bgRow->addWidget(m_bgColorButton);
    bgRow->addStretch();

    cardLayout->addLayout(bgRow);
    layout->addWidget(card);
    layout->addStretch();

    connect(m_bgColorButton, &QPushButton::clicked, this, &ViewportSettingsPanel::onChangeBgColor);
    connect(m_widthSpin, &QSpinBox::editingFinished, this, &ViewportSettingsPanel::onCanvasSizeEdited);
    connect(m_heightSpin, &QSpinBox::editingFinished, this, &ViewportSettingsPanel::onCanvasSizeEdited);
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, &ViewportSettingsPanel::refreshInfo);
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, this, &ViewportSettingsPanel::refreshInfo);

    refreshInfo();
}

void ViewportSettingsPanel::refreshInfo()
{
    m_refreshing = true;

    auto *project = ProjectManager::instance();
    const int width = project->getCanvasWidth();
    const int height = project->getCanvasHeight();

    if (width > 0 && height > 0) {
        m_widthSpin->setValue(width);
        m_heightSpin->setValue(height);
        m_sizeLabel->setText(QString("%1 x %2 px").arg(width).arg(height));
    } else {
        m_sizeLabel->setText(QStringLiteral("—"));
    }

    const QColor background = project->getBackgroundColor();
    m_bgColorButton->setStyleSheet(QString(
        "QPushButton#SettingsColorButton {"
        "background-color: %1;"
        "border-radius: 14px;"
        "padding: 0px;"
        "}").arg(background.name()));

    m_refreshing = false;
}

void ViewportSettingsPanel::onChangeBgColor()
{
    auto *project = ProjectManager::instance();
    const QColor current = project->getBackgroundColor();
    const QColor selected = QColorDialog::getColor(current, this, "Цвет фона");
    if (!selected.isValid() || selected == current)
        return;

    project->setBackgroundColor(selected);
    refreshInfo();
    emit bgColorChanged(selected);
}

void ViewportSettingsPanel::onCanvasSizeEdited()
{
    if (m_refreshing)
        return;

    const int width = m_widthSpin ? m_widthSpin->value() : 1;
    const int height = m_heightSpin ? m_heightSpin->value() : 1;
    ProjectManager::instance()->setCanvasSize(width, height);
    emit canvasSizeChanged(width, height);
}
