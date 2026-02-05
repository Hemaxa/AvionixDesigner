/**
 * @file ViewportSettingsPanel.cpp
 * @brief Реализация панели настроек отображения
 */

#include "ViewportSettingsPanel.h"
#include <QVBoxLayout>
#include <QLabel>

ViewportSettingsPanel::ViewportSettingsPanel(QWidget *parent)
    : BasePanel(parent)
{
    // Устанавливаем имя для стилизации через QSS
    setPanelName("ViewportSettingsPanel");
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    
    // Заглушка на будущее
    m_placeholderLabel = new QLabel("Настройки сцены\n(в разработке)", this);
    m_placeholderLabel->setObjectName("SettingsPlaceholderLabel");
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_placeholderLabel);
}
