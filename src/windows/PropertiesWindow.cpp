/**
 * @file PropertiesWindow.cpp
 * @brief Реализация окна свойств
 */

#include "PropertiesWindow.h"
#include "../managers/ProjectManager.h"
#include "../objects/AbstractObject.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>

PropertiesWindow::PropertiesWindow(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(200);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    m_titleLabel = new QLabel("Свойства объекта", this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    mainLayout->addWidget(m_titleLabel);
    
    m_formLayout = new QFormLayout();
    m_formLayout->setSpacing(6);
    m_formLayout->setLabelAlignment(Qt::AlignRight);
    mainLayout->addLayout(m_formLayout);
    
    mainLayout->addStretch();
}

void PropertiesWindow::showObjectProperties(int index)
{
    auto obj = ProjectManager::instance()->getObjectAt(index);
    showProperties(obj);
}

void PropertiesWindow::showProperties(QSharedPointer<AbstractObject> obj)
{
    clearForm();
    
    if (!obj) {
        m_titleLabel->setText("Объект не выбран");
        return;
    }
    
    m_titleLabel->setText(obj->typeName());
    
    const auto props = obj->getProperties();
    for (const auto &prop : props) {
        addProperty(prop.first, prop.second);
    }
}

void PropertiesWindow::clearProperties()
{
    clearForm();
    m_titleLabel->setText("Свойства объекта");
}

void PropertiesWindow::addProperty(const QString &name, const QString &value)
{
    QLabel *valueLabel = new QLabel(value, this);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_formLayout->addRow(name + ":", valueLabel);
}

void PropertiesWindow::clearForm()
{
    while (m_formLayout->count() > 0) {
        QLayoutItem *item = m_formLayout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}
