#include "ObjectListPanel.h"
#include "ProjectManager.h"
#include <QVBoxLayout>
#include <QListWidget>

ObjectListPanel::ObjectListPanel(QWidget *parent) : BasePanel(parent)
{
    //устанавливаем имя для стилизации через QSS
    setPanelName("ObjectListPanel");
    
    //создаем layout
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    //создаем виджет списка
    m_listWidget = new QListWidget(this);
    m_listWidget->setObjectName("ObjectListWidget");
    m_listWidget->setMinimumWidth(150);
    layout->addWidget(m_listWidget);
    
    //подключаем сигналы
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, &ObjectListPanel::refreshList);
    
    connect(m_listWidget, &QListWidget::currentRowChanged, this, &ObjectListPanel::onRowChanged);
}

void ObjectListPanel::refreshList()
{
    m_listWidget->clear();
    
    auto pm = ProjectManager::instance();
    const auto &objects = pm->getObjects();
    
    //заполняем список объектами
    for (int i = 0; i < objects.size(); ++i) {
        const auto &obj = objects[i];
        
        QString text = QString("%1. %2")
            .arg(i + 1)
            .arg(obj->getTypeName());
        
        QListWidgetItem *item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, i);
        m_listWidget->addItem(item);
    }
}

void ObjectListPanel::onRowChanged(int row)
{
    if (row >= 0) {
        emit objectSelected(row);
    }
}
