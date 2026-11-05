// BasePanel - базовый класс для всех панелей приложения.

#pragma once

#include <QWidget>

// BasePanel - базовый класс для всех панелей приложения.
class BasePanel : public QWidget
{
    Q_OBJECT

public:
    explicit BasePanel(QWidget *parent = nullptr) : QWidget(parent)
    {
        // устанавливаем политику размера по умолчанию
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    virtual ~BasePanel() = default;

    // устанавливает имя панели для стилизации через QSS
    void setPanelName(const QString &name)
    {
        setObjectName(name);
    }
};
