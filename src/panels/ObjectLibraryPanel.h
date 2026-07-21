//ObjectLibraryPanel - компактная библиотека добавления поддерживаемых объектов

#pragma once

#include "BasePanel.h"

#include <QList>

class QHBoxLayout;
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
    void createButtons();
    QToolButton* createLibraryCard(const QString &typeName, const QString &iconPath, const QString &title);

    QHBoxLayout *m_rowLayout = nullptr;
    QList<QToolButton*> m_libraryCards;
};
