//ViewportSettingsPanel - компактная панель параметров рабочей области

#include "ViewportSettingsPanel.h"

#include "ProjectManager.h"
#include "FlowLayout.h"

#include <QColorDialog>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QToolButton>

namespace {
QString toolTipWithShortcut(const QString &text, const QString &shortcutText)
{
    if (shortcutText.isEmpty())
        return text;
    return QStringLiteral("%1 (%2)").arg(text, shortcutText);
}

QToolButton* createSettingsToggle(const QString &iconPath, const QString &toolTip, QWidget *parent, const QString &shortcutText = QString())
{
    auto *button = new QToolButton(parent);
    button->setObjectName("SettingsToolButton");
    button->setCheckable(true);
    button->setIcon(QIcon(iconPath));
    button->setToolTip(toolTipWithShortcut(toolTip, shortcutText));
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}

QIcon createColorSwatchIcon(const QColor &color)
{
    QPixmap pixmap(28, 28);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(QPen(QColor(230, 245, 255, 170), 1));
    painter.drawRoundedRect(QRectF(3, 3, 22, 22), 5, 5);
    return QIcon(pixmap);
}
}

ViewportSettingsPanel::ViewportSettingsPanel(QWidget *parent) : BasePanel(parent)
{
    setPanelName("ViewportSettingsPanel");

    auto *flow = createFlowPanel(this);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(1, 8192);
    m_widthSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_widthSpin->setMinimumWidth(56);
    m_widthSpin->setMaximumWidth(72);
    m_widthSpin->setToolTip(QStringLiteral("Ширина холста (px)"));
    flow->addWidget(m_widthSpin);

    auto *multiplyLabel = new QLabel("×", this);
    multiplyLabel->setObjectName("SettingsFieldLabel");
    flow->addWidget(multiplyLabel);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(1, 8192);
    m_heightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_heightSpin->setMinimumWidth(56);
    m_heightSpin->setMaximumWidth(72);
    m_heightSpin->setToolTip(QStringLiteral("Высота холста (px)"));
    flow->addWidget(m_heightSpin);

    m_bgColorButton = new QPushButton(this);
    m_bgColorButton->setObjectName("SettingsColorButton");
    m_bgColorButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_bgColorButton->setToolTip(QStringLiteral("Цвет фона"));
    flow->addWidget(m_bgColorButton);

    m_gridButton = createSettingsToggle(QStringLiteral(":/icons/icons/settings/grid.svg"), QStringLiteral("Показать сетку"), this, QStringLiteral("Shift+V"));
    m_snapCanvasButton = createSettingsToggle(QStringLiteral(":/icons/icons/settings/snap-screen.svg"), QStringLiteral("Привязка к границам экрана"), this, QStringLiteral("Shift+C"));
    m_snapGridButton = createSettingsToggle(QStringLiteral(":/icons/icons/settings/snap-grid.svg"), QStringLiteral("Привязка к сетке"), this, QStringLiteral("Shift+G"));
    m_snapObjectsButton = createSettingsToggle(QStringLiteral(":/icons/icons/settings/snap-objects.svg"), QStringLiteral("Привязка к другим объектам"), this, QStringLiteral("Shift+O"));

    flow->addWidget(m_gridButton);
    flow->addWidget(m_snapCanvasButton);
    flow->addWidget(m_snapGridButton);
    flow->addWidget(m_snapObjectsButton);


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
    m_bgColorButton->setIcon(createColorSwatchIcon(background));

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
