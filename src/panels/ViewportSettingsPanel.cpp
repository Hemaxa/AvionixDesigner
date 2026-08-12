#include "ViewportSettingsPanel.h"

#include "ProjectManager.h"

#include <QBoxLayout>
#include <QColorDialog>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
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

bool shouldUseVerticalControls(const QWidget *widget)
{
    return widget && widget->height() > widget->width() * 1.15;
}
}

ViewportSettingsPanel::ViewportSettingsPanel(QWidget *parent) : BasePanel(parent)
{
    setPanelName("ViewportSettingsPanel");

    m_layout = new QBoxLayout(QBoxLayout::LeftToRight, this);
    m_layout->setContentsMargins(10, 8, 10, 8);
    m_layout->setSpacing(12);
    m_layout->setAlignment(Qt::AlignCenter);

    // --- Левый столбец: рабочая область ---
    m_canvasLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_canvasLayout->setSpacing(6);
    m_canvasLayout->setAlignment(Qt::AlignCenter);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(1, 8192);
    m_widthSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_widthSpin->setMinimumWidth(56);
    m_widthSpin->setMaximumWidth(72);
    m_widthSpin->setToolTip(QStringLiteral("Ширина холста (px)"));
    m_canvasLayout->addWidget(m_widthSpin, 0, Qt::AlignCenter);

    auto *multiplyLabel = new QLabel("×", this);
    multiplyLabel->setObjectName("SettingsFieldLabel");
    m_canvasLayout->addWidget(multiplyLabel, 0, Qt::AlignCenter);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(1, 8192);
    m_heightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_heightSpin->setMinimumWidth(56);
    m_heightSpin->setMaximumWidth(72);
    m_heightSpin->setToolTip(QStringLiteral("Высота холста (px)"));
    m_canvasLayout->addWidget(m_heightSpin, 0, Qt::AlignCenter);

    m_bgColorButton = new QPushButton(this);
    m_bgColorButton->setObjectName("SettingsColorButton");
    m_bgColorButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_bgColorButton->setToolTip(QStringLiteral("Цвет фона"));
    m_canvasLayout->addWidget(m_bgColorButton, 0, Qt::AlignCenter);

    m_layout->addLayout(m_canvasLayout);

    // --- Разделитель ---
    m_separator = new QFrame(this);
    m_separator->setFrameShape(QFrame::VLine);
    m_separator->setObjectName("SettingsSeparator");
    m_layout->addWidget(m_separator, 0, Qt::AlignCenter);

    // --- Правый столбец: привязки ---
    m_snapLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_snapLayout->setSpacing(6);
    m_snapLayout->setAlignment(Qt::AlignCenter);

    m_gridButton = createSettingsToggle(QStringLiteral(":/icons/icons/settings/grid.svg"), QStringLiteral("Показать сетку"), this, QStringLiteral("Shift+V"));
    m_snapCanvasButton = createSettingsToggle(QStringLiteral(":/icons/icons/settings/snap-screen.svg"), QStringLiteral("Привязка к границам экрана"), this, QStringLiteral("Shift+C"));
    m_snapGridButton = createSettingsToggle(QStringLiteral(":/icons/icons/settings/snap-grid.svg"), QStringLiteral("Привязка к сетке"), this, QStringLiteral("Shift+G"));
    m_snapObjectsButton = createSettingsToggle(QStringLiteral(":/icons/icons/settings/snap-objects.svg"), QStringLiteral("Привязка к другим объектам"), this, QStringLiteral("Shift+O"));

    m_snapLayout->addWidget(m_gridButton, 0, Qt::AlignCenter);
    m_snapLayout->addWidget(m_snapCanvasButton, 0, Qt::AlignCenter);
    m_snapLayout->addWidget(m_snapGridButton, 0, Qt::AlignCenter);
    m_snapLayout->addWidget(m_snapObjectsButton, 0, Qt::AlignCenter);

    m_layout->addLayout(m_snapLayout);
    m_layout->addStretch();

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
    updateAdaptiveLayout();
}

void ViewportSettingsPanel::resizeEvent(QResizeEvent *event)
{
    BasePanel::resizeEvent(event);
    updateAdaptiveLayout();
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

void ViewportSettingsPanel::updateAdaptiveLayout()
{
    if (!m_layout || !m_canvasLayout || !m_snapLayout || !m_separator)
        return;

    const bool vertical = shouldUseVerticalControls(this);
    if (m_verticalLayout == vertical)
        return;

    m_verticalLayout = vertical;
    const QBoxLayout::Direction direction = vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
    m_layout->setDirection(direction);
    m_canvasLayout->setDirection(direction);
    m_snapLayout->setDirection(direction);
    m_separator->setFrameShape(vertical ? QFrame::HLine : QFrame::VLine);
    m_layout->setAlignment(Qt::AlignCenter);
    m_canvasLayout->setAlignment(Qt::AlignCenter);
    m_snapLayout->setAlignment(Qt::AlignCenter);
    updateGeometry();
}
