#include "ObjectPropertiesPanel.h"
#include "ProjectManager.h"
#include "BaseObject.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QTableWidget>
#include <QHeaderView>

ObjectPropertiesPanel::ObjectPropertiesPanel(QWidget *parent) : BasePanel(parent)
{
    //устанавливаем имя для стилизации через QSS
    setPanelName("ObjectPropertiesPanel");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    //создаем область прокрутки
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("PropertiesScrollArea");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    
    //создаем контейнер для содержимого
    QWidget *container = new QWidget();
    container->setObjectName("PropertiesContainer");
    
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(8, 8, 8, 8);
    containerLayout->setSpacing(8);
    
    //заголовок панели
    m_titleLabel = new QLabel("Свойства объекта", container);
    m_titleLabel->setObjectName("PropertiesTitleLabel");
    containerLayout->addWidget(m_titleLabel);
    
    //таблица свойств (2 колонки: Параметр, Значение)
    m_tableWidget = new QTableWidget(0, 2, container);
    m_tableWidget->setObjectName("PropertiesTable");
    m_tableWidget->setHorizontalHeaderLabels({"Параметр", "Значение"});
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    containerLayout->addWidget(m_tableWidget);
    
    m_scrollArea->setWidget(container);
    mainLayout->addWidget(m_scrollArea);
    
    //подключаем сигнал редактирования
    connect(m_tableWidget, &QTableWidget::cellChanged, this, &ObjectPropertiesPanel::onCellChanged);
}

void ObjectPropertiesPanel::showObjectProperties(int index)
{
    auto obj = ProjectManager::instance()->getObjectAt(index);
    showProperties(obj);
}

void ObjectPropertiesPanel::showProperties(QSharedPointer<BaseObject> obj)
{
    clearTable();
    m_currentObject = obj;
    
    if (!obj) {
        m_titleLabel->setText("Объект не выбран");
        return;
    }
    
    //показываем тип объекта
    m_titleLabel->setText(obj->getTypeName());
    
    //заполняем таблицу свойствами
    populateTable(obj->getProperties());
}

void ObjectPropertiesPanel::clearProperties()
{
    clearTable();
    m_currentObject.reset();
    m_titleLabel->setText("Свойства объекта");
}

void ObjectPropertiesPanel::populateTable(const QList<QPair<QString, QString>> &props)
{
    //блокируем сигналы чтобы программное заполнение не вызывало onCellChanged
    m_updating = true;
    
    m_tableWidget->setRowCount(props.size());
    
    for (int i = 0; i < props.size(); ++i) {
        //колонка "Параметр" — только для чтения
        QTableWidgetItem *nameItem = new QTableWidgetItem(props[i].first);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_tableWidget->setItem(i, 0, nameItem);
        
        //колонка "Значение" — редактируемая
        QTableWidgetItem *valueItem = new QTableWidgetItem(props[i].second);
        m_tableWidget->setItem(i, 1, valueItem);
    }
    
    m_updating = false;
}

void ObjectPropertiesPanel::clearTable()
{
    m_updating = true;
    m_tableWidget->setRowCount(0);
    m_updating = false;
}

void ObjectPropertiesPanel::onCellChanged(int row, int column)
{
    //игнорируем программные изменения и изменения в колонке имён
    if (m_updating || column != 1 || !m_currentObject) return;
    
    //получаем имя параметра и новое значение
    QTableWidgetItem *nameItem = m_tableWidget->item(row, 0);
    QTableWidgetItem *valueItem = m_tableWidget->item(row, 1);
    if (!nameItem || !valueItem) return;
    
    QString propName = nameItem->text();
    QString newValue = valueItem->text();
    
    //устанавливаем свойство объекта
    if (m_currentObject->setObjectProperty(propName, newValue)) {
        //перерисовка при изменении
        emit propertyChanged();
    }
    else {
        //если не удалось — возвращаем старое значение
        m_updating = true;
        const auto props = m_currentObject->getProperties();
        for (const auto &prop : props) {
            if (prop.first == propName) {
                valueItem->setText(prop.second);
                break;
            }
        }
        m_updating = false;
    }
}
