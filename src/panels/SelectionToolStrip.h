//SelectionToolStrip - компактная вертикальная панель быстрых действий над выделением

#pragma once

#include "BasePanel.h"

#include <QList>

class QToolButton;

class SelectionToolStrip : public BasePanel
{
    Q_OBJECT

public:
    enum ActionId
    {
        AlignTop = 0,
        AlignVCenter,
        AlignBottom,
        AlignLeft,
        AlignHCenter,
        AlignRight,
        SendToFront,
        SendToBack
    };

    explicit SelectionToolStrip(QWidget *parent = nullptr);

public slots:
    void setSelectionActive(bool active);
    void setHistoryAvailable(bool canUndo, bool canRedo);
    void setPasteAvailable(bool canPaste);

signals:
    void alignRequested(int actionId);
    void undoRequested();
    void redoRequested();
    void copyRequested();
    void pasteRequested();
    void deleteRequested();
    void exportRequested();

private:
    QToolButton* createActionButton(const QString &iconPath, const QString &toolTip);

    QList<QToolButton*> m_buttons;
    QList<QToolButton*> m_selectionButtons;
    QToolButton *m_undoButton = nullptr;
    QToolButton *m_redoButton = nullptr;
    QToolButton *m_copyButton = nullptr;
    QToolButton *m_pasteButton = nullptr;
    QToolButton *m_exportButton = nullptr;
    QToolButton *m_deleteButton = nullptr;
};
