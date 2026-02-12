//BasePanel - базовый класс для всех панелей приложения

#pragma once

#include <QWidget>

class BasePanel : public QWidget
{
    Q_OBJECT
    
public:
    explicit BasePanel(QWidget *parent = nullptr) : QWidget(parent) 
    {
        //устанавливаем политику размера по умолчанию
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    virtual ~BasePanel() = default;
    
    //устанавливает имя панели для стилизации через QSS
    void setPanelName(const QString &name) 
    {
        setObjectName(name);
    }
    
    //геттер для имени панели
    QString getPanelName() const 
    {
        return objectName();
    }

signals:
    void panelActivated();  //сигнал активации панели
};
