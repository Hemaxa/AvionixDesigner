/**
 * @file PropertiesWindow.h
 * @brief Окно свойств выбранного объекта
 */

#pragma once

#include <QWidget>
#include <QSharedPointer>

class QFormLayout;
class QLabel;
class AbstractObject;

/**
 * @class PropertiesWindow
 * @brief Виджет отображения свойств объекта
 */
class PropertiesWindow : public QWidget
{
    Q_OBJECT
    
public:
    explicit PropertiesWindow(QWidget *parent = nullptr);
    
public slots:
    void showObjectProperties(int index);
    void showProperties(QSharedPointer<AbstractObject> obj);
    void clearProperties();

private:
    void addProperty(const QString &name, const QString &value);
    void clearForm();
    
    QLabel *m_titleLabel;
    QFormLayout *m_formLayout;
};
