//EditorWorkspacePanel - центральная рабочая область с холстом и вертикальной панелью действий

#pragma once

#include <QWidget>

class ViewportPanel;
class SelectionToolStrip;

class EditorWorkspacePanel : public QWidget
{
    Q_OBJECT

public:
    explicit EditorWorkspacePanel(QWidget *parent = nullptr);

    ViewportPanel* viewport() const;
    SelectionToolStrip* selectionToolStrip() const;

private:
    ViewportPanel *m_viewport = nullptr;
    SelectionToolStrip *m_selectionToolStrip = nullptr;
};
