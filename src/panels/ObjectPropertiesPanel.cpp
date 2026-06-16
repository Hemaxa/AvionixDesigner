#include "ObjectPropertiesPanel.h"

#include "BaseObject.h"
#include "ProjectManager.h"

#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

ObjectPropertiesPanel::ObjectPropertiesPanel(QWidget *parent) : BasePanel(parent)
{
    setPanelName("ObjectPropertiesPanel");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("PropertiesScrollArea");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    auto *container = new QWidget();
    container->setObjectName("PropertiesContainer");

    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(16, 16, 16, 16);
    containerLayout->setSpacing(12);

    m_titleLabel = new QLabel("Свойства объекта", container);
    m_titleLabel->setObjectName("PropertiesTitleLabel");
    containerLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel("Выберите объект на холсте или в списке, чтобы увидеть и изменить его параметры.", container);
    m_subtitleLabel->setObjectName("PropertiesSubtitleLabel");
    m_subtitleLabel->setWordWrap(true);
    containerLayout->addWidget(m_subtitleLabel);

    m_contentStack = new QStackedWidget(container);
    m_contentStack->setObjectName("PropertiesContentStack");
    containerLayout->addWidget(m_contentStack, 1);

    auto *emptyState = new QFrame(container);
    emptyState->setObjectName("PropertiesEmptyState");
    auto *emptyLayout = new QVBoxLayout(emptyState);
    emptyLayout->setContentsMargins(18, 18, 18, 18);
    emptyLayout->setSpacing(8);

    auto *emptyTitle = new QLabel("Ничего не выбрано", emptyState);
    emptyTitle->setObjectName("PropertiesEmptyTitleLabel");
    emptyLayout->addWidget(emptyTitle);

    m_emptyStateLabel = new QLabel(
        "Панель свойств появится здесь только для выбранного объекта. Это помогает не загромождать интерфейс пустой таблицей.",
        emptyState
    );
    m_emptyStateLabel->setObjectName("PropertiesEmptyTextLabel");
    m_emptyStateLabel->setWordWrap(true);
    emptyLayout->addWidget(m_emptyStateLabel);
    emptyLayout->addStretch();

    m_contentStack->addWidget(emptyState);

    auto *tableContainer = new QFrame(container);
    tableContainer->setObjectName("PropertiesTableCard");
    auto *tableLayout = new QVBoxLayout(tableContainer);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tableLayout->setSpacing(0);

    m_tableWidget = new QTableWidget(0, 2, tableContainer);
    m_tableWidget->setObjectName("PropertiesTable");
    m_tableWidget->setHorizontalHeaderLabels({"Параметр", "Значение"});
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setAlternatingRowColors(false);
    m_tableWidget->setShowGrid(false);
    m_tableWidget->setWordWrap(true);
    m_tableWidget->setCornerButtonEnabled(false);
    tableLayout->addWidget(m_tableWidget);

    m_contentStack->addWidget(tableContainer);
    m_contentStack->setCurrentIndex(0);

    m_scrollArea->setWidget(container);
    mainLayout->addWidget(m_scrollArea);

    connect(m_tableWidget, &QTableWidget::cellChanged, this, &ObjectPropertiesPanel::onCellChanged);
}

void ObjectPropertiesPanel::showObjectProperties(int index)
{
    if (index < 0) {
        clearProperties();
        return;
    }

    auto obj = ProjectManager::instance()->getObjectAt(index);
    showProperties(obj);
}

void ObjectPropertiesPanel::showProperties(QSharedPointer<BaseObject> obj)
{
    clearTable();
    m_currentObject = obj;

    if (!obj) {
        clearProperties();
        return;
    }

    m_titleLabel->setText(obj->getDisplayName());
    m_subtitleLabel->setText("Редактирование значений выполняется прямо в таблице. Изменения сразу отражаются на холсте.");
    populateTable(obj->getProperties());
    m_contentStack->setCurrentIndex(1);
}

void ObjectPropertiesPanel::clearProperties()
{
    clearTable();
    m_currentObject.reset();
    m_titleLabel->setText("Свойства объекта");
    m_subtitleLabel->setText("Выберите объект на холсте или в списке, чтобы увидеть и изменить его параметры.");
    m_contentStack->setCurrentIndex(0);
}

void ObjectPropertiesPanel::populateTable(const QList<QPair<QString, QString>> &props)
{
    m_updating = true;
    m_tableWidget->setRowCount(props.size());

    for (int i = 0; i < props.size(); ++i) {
        auto *nameItem = new QTableWidgetItem(props[i].first);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_tableWidget->setItem(i, 0, nameItem);

        auto *valueItem = new QTableWidgetItem(props[i].second);
        m_tableWidget->setItem(i, 1, valueItem);
    }

    m_tableWidget->resizeRowsToContents();
    m_updating = false;
}

void ObjectPropertiesPanel::clearTable()
{
    m_updating = true;
    m_tableWidget->clearContents();
    m_tableWidget->setRowCount(0);
    m_updating = false;
}

void ObjectPropertiesPanel::onCellChanged(int row, int column)
{
    if (m_updating || column != 1 || !m_currentObject)
        return;

    QTableWidgetItem *nameItem = m_tableWidget->item(row, 0);
    QTableWidgetItem *valueItem = m_tableWidget->item(row, 1);
    if (!nameItem || !valueItem)
        return;

    const QString propName = nameItem->text();
    const QString newValue = valueItem->text();

    if (m_currentObject->setObjectProperty(propName, newValue)) {
        m_updating = true;
        const auto props = m_currentObject->getProperties();
        for (int i = 0; i < props.size(); ++i) {
            if (i < m_tableWidget->rowCount()) {
                if (auto *name = m_tableWidget->item(i, 0)) {
                    name->setText(props[i].first);
                }
                if (auto *value = m_tableWidget->item(i, 1)) {
                    value->setText(props[i].second);
                }
            }
        }
        m_tableWidget->resizeRowsToContents();
        m_updating = false;
        emit propertyChanged();
    }
    else {
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
