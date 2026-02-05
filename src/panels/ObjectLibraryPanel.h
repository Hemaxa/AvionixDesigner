/**
 * @file ObjectLibraryPanel.h
 * @brief Панель библиотеки готовых объектов (заготовка)
 */

#pragma once

#include "../BasePanel.h"

class QLabel;

/**
 * @class ObjectLibraryPanel
 * @brief Библиотека готовых объектов для добавления на сцену
 * @note В будущем здесь появится каталог шаблонов объектов
 */
class ObjectLibraryPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ObjectLibraryPanel(QWidget *parent = nullptr);

private:
    QLabel *m_placeholderLabel;  // Заглушка
};
