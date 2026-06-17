#include "EditorWorkspacePanel.h"

#include "SelectionToolStrip.h"
#include "ViewportPanel.h"

#include <QHBoxLayout>

EditorWorkspacePanel::EditorWorkspacePanel(QWidget *parent) : QWidget(parent)
{
    setObjectName("EditorWorkspacePanel");

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);

    m_viewport = new ViewportPanel(this);
    m_selectionToolStrip = new SelectionToolStrip(this);

    layout->addWidget(m_viewport, 1);
    layout->addWidget(m_selectionToolStrip, 0, Qt::AlignTop);
}

ViewportPanel* EditorWorkspacePanel::viewport() const
{
    return m_viewport;
}

SelectionToolStrip* EditorWorkspacePanel::selectionToolStrip() const
{
    return m_selectionToolStrip;
}
