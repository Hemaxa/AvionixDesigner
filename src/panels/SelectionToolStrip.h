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
        AlignRight
    };

    explicit SelectionToolStrip(QWidget *parent = nullptr);

public slots:
    void setSelectionActive(bool active);

signals:
    void alignRequested(int actionId);
    void deleteRequested();

private:
    QToolButton* createActionButton(const QString &label, const QString &toolTip);

    QList<QToolButton*> m_buttons;
    QToolButton *m_deleteButton = nullptr;
};
