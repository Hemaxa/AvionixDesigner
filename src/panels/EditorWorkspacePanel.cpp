// EditorWorkspacePanel - центральная рабочая область с холстом и вертикальной панелью действий.

#include "EditorWorkspacePanel.h"

#include "SelectionToolStrip.h"
#include "ViewportPanel.h"

#include <QHBoxLayout>
#include <QComboBox>
#include <QSignalBlocker>
#include "ProjectManager.h"

EditorWorkspacePanel::EditorWorkspacePanel(QWidget *parent) : QWidget(parent)
{
    setObjectName("EditorWorkspacePanel");

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);

    m_viewport = new ViewportPanel(this);
    m_selectionToolStrip = new SelectionToolStrip(this);

    // Переключатель поверх рабочей области не резервирует верхнюю полосу.
    auto *combo = new QComboBox(m_viewport);
    combo->setObjectName("WorkspaceLayerCombo");
    combo->setFixedWidth(180);
    combo->move(8, 8);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setToolTip(tr("Выбрать рабочий слой. Управление слоями — в панели «Слои»."));
    auto *project = ProjectManager::instance();
    auto refresh = [combo, project]() {
        const QSignalBlocker blocker(combo);
        combo->clear();
        for (const auto &layer : project->editorLayers()) combo->addItem(layer.name, layer.id);
        combo->setCurrentIndex(combo->findData(project->activeLayerId()));
    };
    connect(project, &ProjectManager::editorLayersChanged, combo, refresh);
    connect(project, &ProjectManager::editorLayerViewChanged, combo, [combo, project]() {
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(combo->findData(project->activeLayerId()));
    });
    connect(project, &ProjectManager::projectLoaded, combo, refresh);
    connect(combo, &QComboBox::activated, this, [combo, project](int index) { project->setActiveLayer(combo->itemData(index).toInt()); });
    refresh();
    layout->addWidget(m_viewport, 1);
    layout->addWidget(m_selectionToolStrip);
}

ViewportPanel* EditorWorkspacePanel::viewport() const
{
    return m_viewport;
}

SelectionToolStrip* EditorWorkspacePanel::selectionToolStrip() const
{
    return m_selectionToolStrip;
}
