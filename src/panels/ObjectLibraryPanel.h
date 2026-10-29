//ObjectLibraryPanel - компактная библиотека добавления поддерживаемых объектов

#pragma once

#include "BasePanel.h"

class FlowLayout;
class QToolButton;

class ObjectLibraryPanel : public BasePanel
{
    Q_OBJECT

public:
    explicit ObjectLibraryPanel(QWidget *parent = nullptr);

signals:
    void objectRequested(const QString &typeName);
    void imageImportRequested(bool rotating);

private:
    void createButtons();
    QToolButton* createLibraryCard(const QString &typeName, const QString &iconPath, const QString &title);

    FlowLayout *m_rowLayout = nullptr;
};
