#ifndef PROPERTIESWIDGET_H
#define PROPERTIESWIDGET_H

#include <QWidget>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSharedPointer>
#include "shapes.h"

/**
 * @brief Виджет для отображения свойств выбранного объекта
 */
class PropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesWidget(QWidget *parent = nullptr);

    // Показать свойства объекта
    void showProperties(QSharedPointer<CorelShape> shape);
    
    // Очистить панель
    void clearProperties();

private:
    QFormLayout *m_layout;
    QLabel *m_titleLabel;
    
    void addProperty(const QString &name, const QString &value);
    void clearLayout();
};

#endif // PROPERTIESWIDGET_H
