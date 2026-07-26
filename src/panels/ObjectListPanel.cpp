#include "ObjectListPanel.h"
#include "ProjectManager.h"
#include <QAbstractItemModel>
#include <algorithm>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSet>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
enum ObjectListRoles
{
  KindRole = Qt::UserRole,
  ObjectIndexRole,
  GroupIdRole
};

enum class RowKind
{
  Object = 0,
  Group = 1
};

QString typeLabelForObject(const BaseObject *object)
{
  if (!object)
    return {};

  const QString typeName = object->getTypeName();
  static const QMap<QString, QString> labels = {
      {QStringLiteral("StaticGroup"), QStringLiteral("static_group")},
      {QStringLiteral("RotationObject"), QStringLiteral("rotation_object")},
      {QStringLiteral("DashedLine"), QStringLiteral("dashed_line")},
      {QStringLiteral("RibbonScale"), QStringLiteral("ribbon_scale")},
      {QStringLiteral("AviaHorizon"), QStringLiteral("avia_horizon")},
      {QStringLiteral("Text"), QStringLiteral("text")},
      {QStringLiteral("Rectangle"), QStringLiteral("rectangle")},
      {QStringLiteral("image"), QStringLiteral("image")}
  };

  if (labels.contains(typeName))
    return labels.value(typeName);

  QString label;
  label.reserve(typeName.size() + 4);
  for (int i = 0; i < typeName.size(); ++i) {
    const QChar ch = typeName.at(i);
    if (i > 0 && ch.isUpper())
      label.append(QLatin1Char('_'));
    label.append(ch.toLower());
  }
  return label;
}

QString objectDisplayName(const QSharedPointer<BaseObject> &object)
{
  if (!object)
    return {};
  return object->customName().isEmpty() ? object->getDisplayName() : object->customName();
}
}

ObjectListPanel::ObjectListPanel(QWidget *parent) : BasePanel(parent) {
  // устанавливаем имя для стилизации через QSS
  setPanelName("ObjectListPanel");

  // создаем layout
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // создаем виджет списка
  m_listWidget = new QListWidget(this);
  m_listWidget->setObjectName("ObjectListWidget");
  m_listWidget->setMinimumWidth(180);
  m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_listWidget->setDragEnabled(true);
  m_listWidget->setAcceptDrops(true);
  m_listWidget->setDropIndicatorShown(true);
  m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
  m_listWidget->setDefaultDropAction(Qt::MoveAction);
  layout->addWidget(m_listWidget);

  // подключаем сигналы
  connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this,
          &ObjectListPanel::refreshList);

  connect(m_listWidget, &QListWidget::currentRowChanged, this,
          &ObjectListPanel::onRowChanged);
  connect(m_listWidget, &QListWidget::itemSelectionChanged, this,
          &ObjectListPanel::onSelectionChanged);
  connect(m_listWidget, &QListWidget::itemDoubleClicked, this,
          &ObjectListPanel::onItemDoubleClicked);
  connect(m_listWidget->model(), &QAbstractItemModel::rowsMoved, this,
          &ObjectListPanel::onRowsMoved);
}

void ObjectListPanel::refreshList() {
  m_refreshing = true;
  const QSignalBlocker blocker(m_listWidget);
  const QList<int> preservedSelection = selectedObjectIndexes();
  m_listWidget->clear();

  auto pm = ProjectManager::instance();
  const auto &objects = pm->getObjects();
  const QList<ProjectManager::ObjectGroup> groups = pm->objectGroups();
  QMap<int, ProjectManager::ObjectGroup> groupByFirstMember;
  QSet<int> groupedMembers;
  for (const auto &group : groups) {
    if (group.members.isEmpty())
      continue;
    QList<int> members = group.members;
    std::sort(members.begin(), members.end());
    groupByFirstMember.insert(members.first(), group);
    for (int member : members)
      groupedMembers.insert(member);
  }

  auto addObjectRow = [&](int index, bool grouped) {
    const auto &obj = objects[index];

    const QString name = objectDisplayName(obj);
    const QString typeText = typeLabelForObject(obj.data());
    const QString text = QString("%1. %2").arg(index + 1).arg(name);

    QListWidgetItem *item = new QListWidgetItem();
    item->setData(KindRole, static_cast<int>(RowKind::Object));
    item->setData(ObjectIndexRole, index);
    item->setData(GroupIdRole, -1);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    m_listWidget->addItem(item);

    auto *rowWidget = new QWidget(m_listWidget);
    rowWidget->setObjectName("ObjectListRowWidget");
    rowWidget->setFixedHeight(36);
    auto *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(grouped ? 24 : 6, 0, 6, 0);
    rowLayout->setSpacing(6);
    rowLayout->setAlignment(Qt::AlignVCenter);

    auto *eyeButton = new QToolButton(rowWidget);
    eyeButton->setObjectName("ObjectListIconButton");
    eyeButton->setIcon(
        QIcon(obj->isViewVisible()
                  ? QStringLiteral(":/icons/icons/list/eye.svg")
                  : QStringLiteral(":/icons/icons/list/eye-off.svg")));
    eyeButton->setToolTip(obj->isViewVisible()
                              ? QStringLiteral("Скрыть на холсте")
                              : QStringLiteral("Показать на холсте"));
    eyeButton->setFixedSize(24, 24);
    eyeButton->setIconSize(QSize(16, 16));
    rowLayout->addWidget(eyeButton, 0, Qt::AlignVCenter);

    auto *label = new QLabel(text, rowWidget);
    label->setObjectName("ObjectListRowLabel");
    label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    label->setWordWrap(false);
    label->setMinimumHeight(24);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    label->setToolTip(text);
    rowLayout->addWidget(label, 1, Qt::AlignVCenter);

    auto *typeLabel = new QLabel(typeText, rowWidget);
    typeLabel->setObjectName("ObjectListTypeLabel");
    typeLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    typeLabel->setToolTip(typeText);
    typeLabel->setMinimumHeight(24);
    typeLabel->setMinimumWidth(82);
    rowLayout->addWidget(typeLabel, 0, Qt::AlignVCenter);

    auto *exportCheck = new QCheckBox(rowWidget);
    exportCheck->setObjectName("ObjectExportCheck");
    exportCheck->setToolTip(QStringLiteral("Экспортировать в XML"));
    exportCheck->setChecked(obj->isExportEnabled());
    exportCheck->setFixedSize(24, 24);
    rowLayout->addWidget(exportCheck, 0, Qt::AlignVCenter);

    item->setSizeHint(QSize(0, 36));
    m_listWidget->setItemWidget(item, rowWidget);

    connect(eyeButton, &QToolButton::clicked, this, [this, index]() {
      auto object = ProjectManager::instance()->getObjectAt(index);
      if (!object)
        return;
      ProjectManager::instance()->setObjectViewVisible(
          index, !object->isViewVisible());
      refreshList();
    });

    connect(exportCheck, &QCheckBox::toggled, this, [index](bool checked) {
      ProjectManager::instance()->setObjectExportEnabled(index, checked);
    });
  };

  auto addGroupRow = [&](const ProjectManager::ObjectGroup &group) {
    QListWidgetItem *item = new QListWidgetItem();
    item->setData(KindRole, static_cast<int>(RowKind::Group));
    item->setData(ObjectIndexRole, -1);
    item->setData(GroupIdRole, group.id);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
    m_listWidget->addItem(item);

    auto *rowWidget = new QWidget(m_listWidget);
    rowWidget->setObjectName("ObjectListGroupRowWidget");
    rowWidget->setFixedHeight(34);
    auto *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(6, 0, 6, 0);
    rowLayout->setSpacing(6);
    rowLayout->setAlignment(Qt::AlignVCenter);

    auto *label = new QLabel(group.name, rowWidget);
    label->setObjectName("ObjectListRowLabel");
    label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    label->setMinimumHeight(24);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    label->setToolTip(group.name);
    rowLayout->addWidget(label, 1, Qt::AlignVCenter);

    auto *typeLabel = new QLabel(QStringLiteral("group"), rowWidget);
    typeLabel->setObjectName("ObjectListTypeLabel");
    typeLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    typeLabel->setMinimumHeight(24);
    typeLabel->setMinimumWidth(82);
    rowLayout->addWidget(typeLabel, 0, Qt::AlignVCenter);

    item->setSizeHint(QSize(0, 34));
    m_listWidget->setItemWidget(item, rowWidget);
  };

  // заполняем список объектами; группы выводятся как служебные строки редактора
  for (int i = 0; i < objects.size(); ++i) {
    if (groupedMembers.contains(i) && !groupByFirstMember.contains(i))
      continue;

    if (groupByFirstMember.contains(i)) {
      const ProjectManager::ObjectGroup group = groupByFirstMember.value(i);
      addGroupRow(group);
      QList<int> members = group.members;
      std::sort(members.begin(), members.end());
      for (int member : members) {
        if (member >= 0 && member < objects.size())
          addObjectRow(member, true);
      }
      continue;
    }

    addObjectRow(i, false);
  }

  int currentRow = -1;
  QSet<int> selectedSet(preservedSelection.begin(), preservedSelection.end());
  for (int row = 0; row < m_listWidget->count(); ++row) {
    auto *item = m_listWidget->item(row);
    if (item->data(KindRole).toInt() != static_cast<int>(RowKind::Object))
      continue;
    const int objectIndex = item->data(ObjectIndexRole).toInt();
    if (!selectedSet.contains(objectIndex))
      continue;
    item->setSelected(true);
    if (currentRow < 0)
      currentRow = row;
  }
  if (currentRow >= 0) {
    m_listWidget->setCurrentRow(currentRow, QItemSelectionModel::NoUpdate);
  }
  m_refreshing = false;
}

void ObjectListPanel::selectRow(int index) {
  selectRows(index >= 0 ? QList<int>{index} : QList<int>{});
}

void ObjectListPanel::selectRows(const QList<int> &indexes) {
  const QSignalBlocker blocker(m_listWidget);
  m_refreshing = true;
  m_listWidget->clearSelection();

  int currentRow = -1;
  QSet<int> selectedSet(indexes.begin(), indexes.end());
  for (int row = 0; row < m_listWidget->count(); ++row) {
    auto *item = m_listWidget->item(row);
    if (!item || item->data(KindRole).toInt() != static_cast<int>(RowKind::Object))
      continue;
    const int objectIndex = item->data(ObjectIndexRole).toInt();
    if (!selectedSet.contains(objectIndex))
      continue;
    item->setSelected(true);
    if (currentRow < 0)
      currentRow = row;
  }

  m_listWidget->setCurrentRow(currentRow, QItemSelectionModel::NoUpdate);
  m_refreshing = false;
}

void ObjectListPanel::onRowChanged(int row) {
  if (m_refreshing)
    return;

  if (m_listWidget->selectedItems().size() <= 1) {
    const auto *item = m_listWidget->item(row);
    if (!item)
      return;
    emit objectSelected(item->data(KindRole).toInt() == static_cast<int>(RowKind::Object)
                        ? item->data(ObjectIndexRole).toInt()
                        : -1);
  }
}

void ObjectListPanel::onSelectionChanged() {
  if (m_refreshing)
    return;

  const QList<int> indexes = selectedObjectIndexes();

  emit selectionChanged(indexes);
  emit objectSelected(indexes.size() == 1 ? indexes.first() : -1);
}

void ObjectListPanel::onRowsMoved(const QModelIndex &parent, int start, int end,
                                  const QModelIndex &destination, int row) {
  Q_UNUSED(parent);
  Q_UNUSED(start);
  Q_UNUSED(end);
  Q_UNUSED(destination);
  Q_UNUSED(row);

  if (m_refreshing)
    return;

  const QList<int> selectedSourceIndexes = selectedObjectIndexes();

  QList<int> order;
  order.reserve(m_listWidget->count());

  for (int row = 0; row < m_listWidget->count(); ++row) {
    QListWidgetItem *item = m_listWidget->item(row);
    if (item->data(KindRole).toInt() == static_cast<int>(RowKind::Object))
      order.append(item->data(ObjectIndexRole).toInt());
  }

  auto *project = ProjectManager::instance();
  if (order.size() != project->getObjectCount() || !project->reorderObjects(order)) {
    refreshList();
    return;
  }

  QList<int> newSelection;
  for (int row = 0; row < order.size(); ++row) {
    if (selectedSourceIndexes.contains(order[row]))
      newSelection.append(row);
  }

  refreshList();
  selectRows(newSelection);
  emit selectionChanged(newSelection);
  emit objectSelected(newSelection.size() == 1 ? newSelection.first() : -1);
}

void ObjectListPanel::onItemDoubleClicked(QListWidgetItem *item)
{
  if (!item)
    return;

  auto *project = ProjectManager::instance();
  const bool isObject = item->data(KindRole).toInt() == static_cast<int>(RowKind::Object);
  const QString title = isObject ? QStringLiteral("Имя объекта") : QStringLiteral("Имя группы");
  QString currentName;
  if (isObject) {
    const int index = item->data(ObjectIndexRole).toInt();
    const auto object = project->getObjectAt(index);
    if (!object)
      return;
    currentName = objectDisplayName(object);
  } else {
    currentName = project->groupName(item->data(GroupIdRole).toInt());
  }

  bool ok = false;
  const QString newName = QInputDialog::getText(this, title, QStringLiteral("Название:"), QLineEdit::Normal, currentName, &ok);
  if (!ok)
    return;

  if (isObject) {
    project->renameObject(item->data(ObjectIndexRole).toInt(), newName);
  } else {
    project->renameGroup(item->data(GroupIdRole).toInt(), newName);
  }
}

QList<int> ObjectListPanel::selectedObjectIndexes() const
{
  QList<int> indexes;
  QSet<int> seen;
  auto *project = ProjectManager::instance();

  for (int row = 0; row < m_listWidget->count(); ++row) {
    const auto *item = m_listWidget->item(row);
    if (!item || !item->isSelected())
      continue;

    if (item->data(KindRole).toInt() == static_cast<int>(RowKind::Object)) {
      const int index = item->data(ObjectIndexRole).toInt();
      if (index >= 0 && !seen.contains(index)) {
        seen.insert(index);
        indexes.append(index);
      }
      continue;
    }

    const QList<int> members = project->groupMembers(item->data(GroupIdRole).toInt());
    for (int index : members) {
      if (index >= 0 && !seen.contains(index)) {
        seen.insert(index);
        indexes.append(index);
      }
    }
  }

  std::sort(indexes.begin(), indexes.end());
  return indexes;
}
