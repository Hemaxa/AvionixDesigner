#include "ObjectListPanel.h"
#include "ProjectManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QToolButton>
#include <QIcon>
#include <QAbstractItemModel>
#include <QSignalBlocker>

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
    m_listWidget->setMinimumWidth(110);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setDragEnabled(true);
    m_listWidget->setAcceptDrops(true);
    m_listWidget->setDropIndicatorShown(true);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);
    layout->addWidget(m_listWidget);
    
    //подключаем сигналы
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, &ObjectListPanel::refreshList);
    
    connect(m_listWidget, &QListWidget::currentRowChanged, this, &ObjectListPanel::onRowChanged);
    connect(m_listWidget->model(), &QAbstractItemModel::rowsMoved, this, &ObjectListPanel::onRowsMoved);
}

void ObjectListPanel::refreshList()
{
    m_refreshing = true;
    const QSignalBlocker blocker(m_listWidget);
    const int selectedRow = m_listWidget->currentRow();
    m_listWidget->clear();
    
    auto pm = ProjectManager::instance();
    const auto &objects = pm->getObjects();
    
    //заполняем список объектами
    for (int i = 0; i < objects.size(); ++i) {
        const auto &obj = objects[i];
        
        QString text = QString("%1. %2")
            .arg(i + 1)
            .arg(obj->getDisplayName());
        
        QListWidgetItem *item = new QListWidgetItem();
        item->setData(Qt::UserRole, i);
        m_listWidget->addItem(item);

        auto *rowWidget = new QWidget(m_listWidget);
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(4, 2, 4, 2);
        rowLayout->setSpacing(6);

        auto *eyeButton = new QToolButton(rowWidget);
        eyeButton->setObjectName("ObjectListIconButton");
        eyeButton->setIcon(QIcon(obj->isViewVisible()
            ? QStringLiteral(":/icons/icons/list/eye.svg")
            : QStringLiteral(":/icons/icons/list/eye-off.svg")));
        eyeButton->setToolTip(obj->isViewVisible() ? QStringLiteral("Скрыть на холсте") : QStringLiteral("Показать на холсте"));
        eyeButton->setFixedSize(24, 24);
        rowLayout->addWidget(eyeButton);

        auto *label = new QLabel(text, rowWidget);
        label->setObjectName("ObjectListRowLabel");
        rowLayout->addWidget(label, 1);

        auto *exportCheck = new QCheckBox(rowWidget);
        exportCheck->setObjectName("ObjectExportCheck");
        exportCheck->setToolTip(QStringLiteral("Экспортировать в XML"));
        exportCheck->setChecked(obj->isExportEnabled());
        rowLayout->addWidget(exportCheck);

        item->setSizeHint(rowWidget->sizeHint());
        m_listWidget->setItemWidget(item, rowWidget);

        connect(eyeButton, &QToolButton::clicked, this, [this, i]() {
            auto object = ProjectManager::instance()->getObjectAt(i);
            if (!object)
                return;
            ProjectManager::instance()->setObjectViewVisible(i, !object->isViewVisible());
            refreshList();
        });

        connect(exportCheck, &QCheckBox::toggled, this, [this, i](bool checked) {
            ProjectManager::instance()->setObjectExportEnabled(i, checked);
        });
    }

    if (selectedRow >= 0 && selectedRow < m_listWidget->count()) {
        m_listWidget->setCurrentRow(selectedRow);
    }
    m_refreshing = false;
}

void ObjectListPanel::selectRow(int index)
{
    if (index >= 0 && index < m_listWidget->count()) {
        m_listWidget->setCurrentRow(index);
    } else {
        m_listWidget->clearSelection();
        m_listWidget->setCurrentRow(-1);
    }
}

void ObjectListPanel::onRowChanged(int row)
{
    if (m_refreshing)
        return;

    emit objectSelected(row);
}

void ObjectListPanel::onRowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    Q_UNUSED(destination);
    Q_UNUSED(row);

    if (m_refreshing)
        return;

    QList<int> order;
    order.reserve(m_listWidget->count());
    int selectedSourceIndex = -1;

    if (QListWidgetItem *currentItem = m_listWidget->currentItem()) {
        selectedSourceIndex = currentItem->data(Qt::UserRole).toInt();
    }

    for (int row = 0; row < m_listWidget->count(); ++row) {
        QListWidgetItem *item = m_listWidget->item(row);
        order.append(item->data(Qt::UserRole).toInt());
    }

    if (!ProjectManager::instance()->reorderObjects(order))
        return;

    int newSelectedRow = -1;
    for (int row = 0; row < order.size(); ++row) {
        if (order[row] == selectedSourceIndex) {
            newSelectedRow = row;
            break;
        }
    }

    refreshList();
    selectRow(newSelectedRow);
    emit objectSelected(newSelectedRow);
}
