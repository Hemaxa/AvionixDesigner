//ObjectLibraryPanel - панель библиотеки готовых объектов


#pragma once

#include "BasePanel.h"

class QPushButton;
class QGridLayout;

class ObjectLibraryPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ObjectLibraryPanel(QWidget *parent = nullptr);

private:
    void createButtons();
    QPushButton* createLibraryButton(const QString &iconPath, const QString &tooltip);
    
    QGridLayout *m_gridLayout;
    
    //кнопки примитивов
    QPushButton *m_rectButton;
    QPushButton *m_circleButton;
    QPushButton *m_lineButton;
    QPushButton *m_polygonButton;
    QPushButton *m_textButton;
    QPushButton *m_imageButton;
};
