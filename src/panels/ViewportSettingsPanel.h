/**
 * @file ViewportSettingsPanel.h
 * @brief Панель настроек отображения сцены (заготовка)
 */

#pragma once

#include "BasePanel.h"

class QLabel;

/**
 * @class ViewportSettingsPanel
 * @brief Настройки отображения сцены (сетка, привязка и т.д.)
 * @note В будущем здесь появятся настройки отображения
 */
class ViewportSettingsPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ViewportSettingsPanel(QWidget *parent = nullptr);

private:
    QLabel *m_placeholderLabel;  // Заглушка
};
