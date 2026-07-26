//SelectionToolStrip - компактная вертикальная панель быстрых действий над выделением

#pragma once

#include "BasePanel.h"

#include <QList>

class QToolButton;

class SelectionToolStrip : public BasePanel
{
    Q_OBJECT

public:
    //идентификаторы команд выравнивания и изменения порядка слоёв
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
    //включает или выключает команды, которым нужно выделение
    void setSelectionActive(bool active);
    //включает кнопки undo/redo по состоянию истории проекта
    void setHistoryAvailable(bool canUndo, bool canRedo);
    //включает кнопку вставки по состоянию внутреннего буфера обмена
    void setPasteAvailable(bool canPaste);

signals:
    //запрашивает выравнивание или изменение порядка слоёв
    void alignRequested(int actionId);
    //запрашивает отмену последнего действия
    void undoRequested();
    //запрашивает повтор отменённого действия
    void redoRequested();
    //запрашивает копирование выделенных объектов
    void copyRequested();
    //запрашивает вставку объектов
    void pasteRequested();
    //запрашивает удаление выделенных объектов
    void deleteRequested();
    //запрашивает создание редакторской группы
    void groupRequested();
    //запрашивает удаление редакторской группы
    void ungroupRequested();

private:
    //создаёт кнопку панели быстрых действий с иконкой и подсказкой
    QToolButton* createActionButton(const QString &iconPath, const QString &toolTip);

    QList<QToolButton*> m_buttons;
    QList<QToolButton*> m_selectionButtons;
    QToolButton *m_undoButton = nullptr;
    QToolButton *m_redoButton = nullptr;
    QToolButton *m_copyButton = nullptr;
    QToolButton *m_pasteButton = nullptr;
    QToolButton *m_deleteButton = nullptr;
};
