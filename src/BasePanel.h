/**
 * @file BasePanel.h
 * @brief Базовый класс для всех панелей приложения
 */

#pragma once

#include <QWidget>

/**
 * @class BasePanel
 * @brief Базовый класс для панелей с поддержкой стилизации
 */
class BasePanel : public QWidget
{
    Q_OBJECT
    
public:
    explicit BasePanel(QWidget *parent = nullptr) 
        : QWidget(parent) 
    {
        // Устанавливаем политику размера по умолчанию
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    
    virtual ~BasePanel() = default;
    
    // Устанавливает имя панели для стилизации через QSS
    void setPanelName(const QString &name) 
    {
        setObjectName(name);
    }
    
    // Возвращает имя панели
    QString panelName() const 
    {
        return objectName();
    }

signals:
    void panelActivated();  // Сигнал активации панели
};
