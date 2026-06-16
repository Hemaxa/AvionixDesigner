//ObjectLibraryPanel - панель библиотеки готовых объектов


#pragma once

#include "BasePanel.h"

#include <QList>

class QGridLayout;
class QLabel;
class QToolButton;

class ObjectLibraryPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ObjectLibraryPanel(QWidget *parent = nullptr);

signals:
    void objectRequested(const QString &typeName);

private:
    void createButtons();
    QToolButton* createLibraryCard(const QString &typeName, const QString &iconPath, const QString &title);
    
    QGridLayout *m_gridLayout = nullptr;
    QLabel *m_descriptionLabel = nullptr;
    
    QList<QToolButton*> m_libraryCards;
};
