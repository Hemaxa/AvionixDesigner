/**
 * @file ObjectLibraryPanel.cpp
 * @brief Реализация панели библиотеки объектов
 */

#include "ObjectLibraryPanel.h"
#include <QVBoxLayout>
#include <QLabel>

ObjectLibraryPanel::ObjectLibraryPanel(QWidget *parent)
    : BasePanel(parent)
{
    // Устанавливаем имя для стилизации через QSS
    setPanelName("ObjectLibraryPanel");
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    
    // Заглушка на будущее
    m_placeholderLabel = new QLabel("Библиотека объектов\n(в разработке)", this);
    m_placeholderLabel->setObjectName("LibraryPlaceholderLabel");
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_placeholderLabel);
}
