#include "ViewportSettingsPanel.h"

#include "ProjectManager.h"

#include <QColorDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QToolButton* createSettingsToggle(const QString &iconPath, const QString &toolTip, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setObjectName("SettingsToolButton");
    button->setCheckable(true);
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(18, 18));
    button->setToolTip(toolTip);
    button->setFixedSize(40, 40);
    return button;
}
}

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
    m_widthSpin->setMinimumWidth(64);
    sizeRow->addWidget(m_widthSpin);

    auto *multiplyLabel = new QLabel("x", card);
    multiplyLabel->setObjectName("SettingsFieldLabel");
    sizeRow->addWidget(multiplyLabel);

    m_heightSpin = new QSpinBox(card);
    m_heightSpin->setRange(1, 8192);
    m_heightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_heightSpin->setMinimumWidth(64);
    sizeRow->addWidget(m_heightSpin);
    sizeRow->addStretch();

    cardLayout->addLayout(sizeRow);

    auto *bgRow = new QHBoxLayout();
    bgRow->setSpacing(6);

    auto *bgCaption = new QLabel("Фон", card);
    bgCaption->setObjectName("SettingsFieldLabel");
    bgRow->addWidget(bgCaption);

    m_bgColorButton = new QPushButton(card);
    m_bgColorButton->setObjectName("SettingsColorButton");
    m_bgColorButton->setFixedSize(40, 40);
    bgRow->addWidget(m_bgColorButton);
    bgRow->addStretch();

    cardLayout->addLayout(bgRow);

    auto *snapCaption = new QLabel(QStringLiteral("Сетка и привязки"), card);
    snapCaption->setObjectName("SettingsFieldLabel");
    cardLayout->addWidget(snapCaption);

    auto *snapRow = new QHBoxLayout();
    snapRow->setSpacing(6);
    m_gridButton = createSettingsToggle(QStringLiteral(":/icons/icons/settings/grid.svg"), QStringLiteral("Показать сетку"), card);
    m_snapCanvasButton = createSettingsToggle(QStringLiteral(":/icons/icons/settings/snap-screen.svg"), QStringLiteral("Привязка к границам экрана"), card);
    m_snapGridButton = createSettingsToggle(QStringLiteral(":/icons/icons/settings/snap-grid.svg"), QStringLiteral("Привязка к сетке"), card);
    m_snapObjectsButton = createSettingsToggle(QStringLiteral(":/icons/icons/settings/snap-objects.svg"), QStringLiteral("Привязка к другим объектам"), card);
    snapRow->addWidget(m_gridButton);
    snapRow->addWidget(m_snapCanvasButton);
    snapRow->addWidget(m_snapGridButton);
    snapRow->addWidget(m_snapObjectsButton);
    snapRow->addStretch();
    cardLayout->addLayout(snapRow);

    layout->addWidget(card);
    layout->addStretch();

    connect(m_bgColorButton, &QPushButton::clicked, this, &ViewportSettingsPanel::onChangeBgColor);
    connect(m_widthSpin, &QSpinBox::editingFinished, this, &ViewportSettingsPanel::onCanvasSizeEdited);
    connect(m_heightSpin, &QSpinBox::editingFinished, this, &ViewportSettingsPanel::onCanvasSizeEdited);
    connect(m_gridButton, &QToolButton::toggled, this, &ViewportSettingsPanel::onToggleGrid);
    connect(m_snapCanvasButton, &QToolButton::toggled, this, &ViewportSettingsPanel::onToggleSnapCanvas);
    connect(m_snapGridButton, &QToolButton::toggled, this, &ViewportSettingsPanel::onToggleSnapGrid);
    connect(m_snapObjectsButton, &QToolButton::toggled, this, &ViewportSettingsPanel::onToggleSnapObjects);
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
    }

    m_gridButton->setChecked(project->showGrid());
    m_snapCanvasButton->setChecked(project->snapToCanvas());
    m_snapGridButton->setChecked(project->snapToGrid());
    m_snapObjectsButton->setChecked(project->snapToObjects());

    const QColor background = project->getBackgroundColor();
    m_bgColorButton->setStyleSheet(QString(
        "QPushButton#SettingsColorButton {"
        "background-color: %1;"
        "border-radius: 9px;"
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

void ViewportSettingsPanel::onToggleGrid(bool enabled)
{
    if (!m_refreshing)
        ProjectManager::instance()->setShowGrid(enabled);
}

void ViewportSettingsPanel::onToggleSnapCanvas(bool enabled)
{
    if (!m_refreshing)
        ProjectManager::instance()->setSnapToCanvas(enabled);
}

void ViewportSettingsPanel::onToggleSnapGrid(bool enabled)
{
    if (!m_refreshing)
        ProjectManager::instance()->setSnapToGrid(enabled);
}

void ViewportSettingsPanel::onToggleSnapObjects(bool enabled)
{
    if (!m_refreshing)
        ProjectManager::instance()->setSnapToObjects(enabled);
}
