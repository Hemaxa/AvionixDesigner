/**
 * @file ObjectPropertiesPanel.cpp
 * @brief Реализация панели свойств объекта
 */

#include "ObjectPropertiesPanel.h"
#include "ProjectManager.h"
#include "BaseObject.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QScrollArea>

ObjectPropertiesPanel::ObjectPropertiesPanel(QWidget *parent)
    : BasePanel(parent)
{
    // Устанавливаем имя для стилизации через QSS
    setPanelName("ObjectPropertiesPanel");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Создаем область прокрутки
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("PropertiesScrollArea");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    
    // Создаем контейнер для содержимого
    QWidget *container = new QWidget();
    container->setObjectName("PropertiesContainer");
    
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(8, 8, 8, 8);
    containerLayout->setSpacing(8);
    
    // Заголовок панели
    m_titleLabel = new QLabel("Свойства объекта", container);
    m_titleLabel->setObjectName("PropertiesTitleLabel");
    containerLayout->addWidget(m_titleLabel);
    
    // Форма для свойств
    m_formLayout = new QFormLayout();
    m_formLayout->setSpacing(6);
    m_formLayout->setLabelAlignment(Qt::AlignRight);
    containerLayout->addLayout(m_formLayout);
    
    containerLayout->addStretch();
    
    m_scrollArea->setWidget(container);
    mainLayout->addWidget(m_scrollArea);
}

void ObjectPropertiesPanel::showObjectProperties(int index)
{
    auto obj = ProjectManager::instance()->getObjectAt(index);
    showProperties(obj);
}

void ObjectPropertiesPanel::showProperties(QSharedPointer<BaseObject> obj)
{
    clearForm();
    
    if (!obj) {
        m_titleLabel->setText("Объект не выбран");
        return;
    }
    
    // Показываем тип объекта
    m_titleLabel->setText(obj->typeName());
    
    // Добавляем свойства
    const auto props = obj->getProperties();
    for (const auto &prop : props) {
        addProperty(prop.first, prop.second);
    }
}

void ObjectPropertiesPanel::clearProperties()
{
    clearForm();
    m_titleLabel->setText("Свойства объекта");
}

void ObjectPropertiesPanel::addProperty(const QString &name, const QString &value)
{
    QLabel *valueLabel = new QLabel(value);
    valueLabel->setObjectName("PropertyValueLabel");
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_formLayout->addRow(name + ":", valueLabel);
}

void ObjectPropertiesPanel::clearForm()
{
    // Удаляем все элементы из формы
    while (m_formLayout->count() > 0) {
        QLayoutItem *item = m_formLayout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}
