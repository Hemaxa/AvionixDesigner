//ObjectLibraryPanel - компактная библиотека добавления поддерживаемых объектов

#pragma once

#include "BasePanel.h"

#include <QList>

class QBoxLayout;
class QResizeEvent;
class QToolButton;

class ObjectLibraryPanel : public BasePanel
{
    Q_OBJECT

public:
    explicit ObjectLibraryPanel(QWidget *parent = nullptr);

signals:
    void objectRequested(const QString &typeName);
    void imageImportRequested();

private:
    void resizeEvent(QResizeEvent *event) override;
    void createButtons();
    QToolButton* createLibraryCard(const QString &typeName, const QString &iconPath, const QString &title);
    void updateAdaptiveLayout();

    QBoxLayout *m_rowLayout = nullptr;
    QList<QToolButton*> m_libraryCards;
    bool m_verticalLayout = false;
};
