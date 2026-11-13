// ObjectPropertiesPanel - панель свойств выбранного объекта.

#include "ObjectPropertiesPanel.h"

#include "BaseObject.h"
#include "ProjectManager.h"
#include "ThreeDViewObject.h"
#include "ImageObject.h"
#include "StaticGroupObject.h"
#include "TextObject.h"
#include "ExpressionSpinBox.h"
#include "fpga/compiler/FpgaResourceLimits.h"

#include <QColorDialog>
#include <QComboBox>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <algorithm>
#include <limits>
#include <QMessageBox>
#include <QScrollArea>
#include <QSet>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QTimer>

namespace {
bool isStaticImage(const QSharedPointer<BaseObject> &object)
{
    if (const auto *image = dynamic_cast<const ImageObject *>(object.data())) return !image->isRotationObject();
    return dynamic_cast<const StaticGroupObject *>(object.data()) && !dynamic_cast<const TextObject *>(object.data());
}

bool isLayerColor(const QString &name)
{
    return (name.startsWith(QStringLiteral("Слой ")) || name.startsWith(QStringLiteral("Состояние ")))
        && name.endsWith(QStringLiteral(": Цвет"));
}

QString layerOrderProperty(const QString &colorProperty)
{
    return colorProperty.left(colorProperty.indexOf(':') + 1) + QStringLiteral(" Порядок");
}

// PropertyValueDelegate - выбор редакторов свойств и размещение составного поля прозрачности.
class PropertyValueDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const auto *table = qobject_cast<QTableWidget *>(parent());
        const auto *editor = table ? table->cellWidget(index.row(), 0) : nullptr;
        if (!editor || editor->objectName() != QStringLiteral("StaticLayerEditor")) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionViewItem item(option);
        initStyleOption(&item, index);
        const auto *style = item.widget ? item.widget->style() : table->style();
        if (index.column() == 0) {
            // Под прозрачным составным полем рисуем фон строки без служебного имени свойства.
            item.text.clear();
            item.features &= ~QStyleOptionViewItem::HasDisplay;
            style->drawControl(QStyle::CE_ItemViewItem, &item, painter, item.widget);
            return;
        }
        const auto icon = item.icon;
        item.icon = QIcon();
        item.features &= ~QStyleOptionViewItem::HasDecoration;
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, item.widget);
        const QRect text = style->subElementRect(QStyle::SE_ItemViewItemText, &item, item.widget);
        // Сохраняем прежний кружочек, располагая его после подписи «Цвет».
        icon.paint(painter, QRect(text.left() + item.fontMetrics.horizontalAdvance(item.text) + 8,
                                 text.center().y() - 8, 16, 16));
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (editor->objectName() == QStringLiteral("TransparencyEditor")
            || editor->objectName() == QStringLiteral("PropertyFieldEditor")
            || editor->objectName() == QStringLiteral("StaticLayerEditor")) {
            // У составного поля свои отступы: используем всю высоту обычной строки,
            // без повторного вычитания padding из стиля ячейки.
            editor->setGeometry(option.rect);
            return;
        }
        if (qobject_cast<QComboBox *>(editor)) {
            QRect rect = option.rect.adjusted(10, 3, -10, -3);
            const int height = qMin(rect.height(), editor->maximumHeight());
            rect.setTop(rect.top() + (rect.height() - height) / 2);
            rect.setHeight(height);
            editor->setGeometry(rect);
            return;
        }
        QStyledItemDelegate::updateEditorGeometry(editor, option, index);
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (index.column() != 1) {
            return QStyledItemDelegate::createEditor(parent, option, index);
        }

        const QString propertyName = index.siblingAtColumn(0).data().toString();
        if (propertyName == QStringLiteral("Положение")) {
            auto *combo = new QComboBox(parent);
            combo->setObjectName("PropertyValueCombo");
            combo->addItems({QStringLiteral("Справа"), QStringLiteral("Слева")});
            return combo;
        }
        if (propertyName == QStringLiteral("Шрифт")) {
            auto *combo = new QComboBox(parent);
            combo->setObjectName("PropertyValueCombo");
            combo->addItem(QStringLiteral("Arial"));
            combo->addItem(QStringLiteral("Times New Roman"));
            combo->addItem(QStringLiteral("Helvetica"));
            combo->addItem(QStringLiteral("Courier New"));
            combo->addItem(QStringLiteral("Verdana"));
            combo->addItem(QStringLiteral("Tahoma"));
            combo->addItem(QStringLiteral("Georgia"));
            return combo;
        }

        const QSet<QString> boolNames = {
            QStringLiteral("Включен"),
            QStringLiteral("Видимость"),
            QStringLiteral("Автоцвет")
        };

        if (boolNames.contains(propertyName) || propertyName.endsWith(QStringLiteral("Видимость"))) {
            auto *combo = new QComboBox(parent);
            combo->setObjectName("PropertyValueCombo");
            combo->addItem(QStringLiteral("да"));
            combo->addItem(QStringLiteral("нет"));
            return combo;
        }

        return QStyledItemDelegate::createEditor(parent, option, index);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox*>(editor)) {
            const QString current = index.data().toString().trimmed().toLower();
            const int exactIndex = combo->findText(index.data().toString());
            if (exactIndex >= 0) {
                combo->setCurrentIndex(exactIndex);
            } else {
                combo->setCurrentIndex(index.data(Qt::UserRole).toBool() ? -1 : (current == QStringLiteral("нет") ? 1 : 0));
            }
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
        if (index.data(Qt::UserRole).toBool()) {
            if (auto *line = qobject_cast<QLineEdit *>(editor)) {
                line->clear();
                line->setPlaceholderText(QObject::tr("Разные значения"));
            }
        }
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox*>(editor)) {
            if (combo->currentIndex() >= 0) model->setData(index, combo->currentText());
            return;
        }
        if (index.data(Qt::UserRole).toBool()) {
            if (auto *line = qobject_cast<QLineEdit *>(editor); line && !line->isModified()) return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }
};
}

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
    containerLayout->setContentsMargins(8, 8, 8, 8);
    containerLayout->setSpacing(8);

    m_titleLabel = new QLabel(container);
    m_titleLabel->setObjectName("PropertiesTitleLabel");
    containerLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel(container);
    m_subtitleLabel->setObjectName("PropertiesHardwareWarning");
    m_subtitleLabel->setTextFormat(Qt::PlainText);
    m_subtitleLabel->setWordWrap(true);
    m_subtitleLabel->hide();
    containerLayout->addWidget(m_subtitleLabel);

    m_contentStack = new QStackedWidget(container);
    m_contentStack->setObjectName("PropertiesContentStack");
    containerLayout->addWidget(m_contentStack, 1);

    auto *emptyState = new QFrame(container);
    emptyState->setObjectName("PropertiesEmptyState");
    auto *emptyLayout = new QVBoxLayout(emptyState);
    emptyLayout->setContentsMargins(14, 14, 14, 14);
    emptyLayout->setSpacing(4);

    auto *emptyTitle = new QLabel("Объект не выбран", emptyState);
    emptyTitle->setObjectName("PropertiesEmptyTitleLabel");
    emptyLayout->addWidget(emptyTitle);

    m_emptyStateLabel = new QLabel("Выделите элемент на холсте.", emptyState);
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
    m_tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setAlternatingRowColors(false);
    m_tableWidget->setShowGrid(false);
    m_tableWidget->setWordWrap(false);
    m_tableWidget->setTextElideMode(Qt::ElideRight);
    m_tableWidget->setCornerButtonEnabled(false);
    m_tableWidget->setItemDelegateForColumn(0, new PropertyValueDelegate(m_tableWidget));
    m_tableWidget->setItemDelegateForColumn(1, new PropertyValueDelegate(m_tableWidget));
    tableLayout->addWidget(m_tableWidget);

    m_contentStack->addWidget(tableContainer);
    m_contentStack->setCurrentIndex(0);
    m_titleLabel->hide();

    m_scrollArea->setWidget(container);
    mainLayout->addWidget(m_scrollArea);

    connect(m_tableWidget, &QTableWidget::cellChanged, this, &ObjectPropertiesPanel::onCellChanged);
    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, &ObjectPropertiesPanel::onCellDoubleClicked);
    auto refresh = [this]() {
        QTimer::singleShot(0, this, [this]() {
            const auto &objects = ProjectManager::instance()->getObjects();
            for (const auto &object : m_currentObjects) {
                if (!objects.contains(object)) { clearProperties(); return; }
            }
            refreshTableValues();
        });
    };
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, this, [refresh]() {
        if (!ProjectManager::instance()->isAircraftStateChange()) refresh();
    });
    connect(ProjectManager::instance(), &ProjectManager::editorLayersChanged, this, refresh);
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

void ObjectPropertiesPanel::showSelectedObjects(const QList<int> &indexes)
{
    QList<QSharedPointer<BaseObject>> objects;
    for (int index : indexes) {
        auto object = ProjectManager::instance()->getObjectAt(index);
        if (object && !objects.contains(object)) objects.append(object);
    }
    if (objects.isEmpty()) { clearProperties(); return; }
    if (m_currentObjects == objects) { refreshTableValues(); return; }
    for (const auto &connection : m_objectConnections) disconnect(connection);
    m_objectConnections.clear();
    clearTable();
    m_currentObjects = objects;
    m_currentObject = objects.first();
    m_titleLabel->setText(objects.size() == 1 ? objects.first()->getDisplayName() : tr("Выбрано объектов: %1").arg(objects.size()));
    m_titleLabel->show();
    populateTable(commonProperties());
    refreshHardwareWarnings();
    for (const auto &object : objects)
        m_objectConnections.append(connect(object.data(), &BaseObject::changed, this, &ObjectPropertiesPanel::refreshHardwareWarnings));
    m_contentStack->setCurrentIndex(1);
}

void ObjectPropertiesPanel::showProperties(QSharedPointer<BaseObject> obj)
{
    const int index = ProjectManager::instance()->getObjects().indexOf(obj);
    showSelectedObjects(index >= 0 ? QList<int>{index} : QList<int>{});
}

void ObjectPropertiesPanel::clearProperties()
{
    for (const auto &connection : m_objectConnections) disconnect(connection);
    m_objectConnections.clear();
    clearTable();
    m_currentObjects.clear();
    m_currentObject.reset();
    m_titleLabel->clear();
    m_titleLabel->hide();
    m_subtitleLabel->clear();
    m_subtitleLabel->hide();
    m_contentStack->setCurrentIndex(0);
}

QList<QPair<QString, QString>> ObjectPropertiesPanel::commonProperties() const
{
    m_mixedProperties.clear();
    if (m_currentObjects.isEmpty()) return {};
    auto props = m_currentObjects.first()->getProperties();
    QList<QMap<QString, QString>> propertyMaps;
    for (const auto &object : m_currentObjects) {
        QMap<QString, QString> map;
        for (const auto &property : object->getProperties()) map.insert(property.first, property.second);
        propertyMaps.append(map);
    }
    for (int i = props.size() - 1; i >= 0; --i) {
        bool common = true, mixed = false;
        for (const auto &map : propertyMaps) {
            const auto it = map.constFind(props[i].first);
            if (it == map.cend()) { common = false; break; }
            if (it.value() != props[i].second) mixed = true;
        }
        if (!common) props.removeAt(i);
        else if (mixed) { m_mixedProperties.insert(props[i].first); props[i].second = tr("Разные значения"); }
    }
    const int layer = m_currentObjects.first()->editorLayerId();
    const bool sameLayer = std::all_of(m_currentObjects.cbegin(), m_currentObjects.cend(), [layer](const auto &o) { return o->editorLayerId() == layer; });
    if (!sameLayer) m_mixedProperties.insert(tr("Слой"));
    props.append({tr("Слой"), sameLayer ? ProjectManager::instance()->editorLayerName(layer) : tr("Разные значения")});
    // Порядок блока редактируется рядом с его цветом в одной строке.
    if (std::all_of(m_currentObjects.cbegin(), m_currentObjects.cend(), isStaticImage))
        props.erase(std::remove_if(props.begin(), props.end(), [](const auto &property) {
            return property.first.endsWith(QStringLiteral(": Порядок"));
        }), props.end());
    return props;
}

void ObjectPropertiesPanel::populateTable(const QList<QPair<QString, QString>> &props)
{
    m_updating = true;
    m_tableWidget->setRowCount(props.size());
    m_readOnlyProperties.clear();
    for (const auto &property : props) {
        if (property.first == tr("Слой")) continue;
        for (const auto &object : m_currentObjects) {
            if (!ProjectManager::instance()->isObjectPropertyEditable(object.data(), property.first)) {
                m_readOnlyProperties.insert(property.first);
                break;
            }
        }
    }
    for (int i = 0; i < props.size(); ++i) {
        auto *nameItem = new QTableWidgetItem(props[i].first);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_tableWidget->setItem(i, 0, nameItem);
        m_tableWidget->setItem(i, 1, new QTableWidgetItem());
        if (isLayerColor(props[i].first) && std::all_of(m_currentObjects.cbegin(), m_currentObjects.cend(), isStaticImage)) {
            auto *editor = new QWidget(m_tableWidget);
            editor->setObjectName("StaticLayerEditor");
            auto *layout = new QHBoxLayout(editor);
            layout->setContentsMargins(13, 3, 6, 3);
            layout->setSpacing(6);
            layout->addWidget(new QLabel(tr("Слой"), editor));
            auto *combo = new QComboBox(editor);
            combo->setObjectName("StaticLayerOrderCombo");
            combo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            combo->setMinimumContentsLength(2);
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            combo->setToolTip(tr("1 — передний план (индекс параметра 0). Больший номер — дальше от переднего плана."));
            combo->setPlaceholderText(tr("Разные"));
            const auto orderProperty = layerOrderProperty(props[i].first);
            combo->setEnabled(std::all_of(m_currentObjects.cbegin(), m_currentObjects.cend(), [&orderProperty](const auto &object) {
                return ProjectManager::instance()->isObjectPropertyEditable(object.data(), orderProperty);
            }));
            layout->addWidget(combo);
            layout->addStretch();
            nameItem->setSizeHint(editor->sizeHint());
            m_tableWidget->setCellWidget(i, 0, editor);
            connect(combo, &QComboBox::activated, this, [this, combo, orderProperty](int index) {
                if (!m_updating && index >= 0) applyProperty(orderProperty, combo->itemData(index).toString());
            });
        }
        if (props[i].first == tr("Слой")) {
            auto *editor = new QWidget(m_tableWidget);
            editor->setObjectName("PropertyFieldEditor");
            auto *layout = new QHBoxLayout(editor);
            layout->setContentsMargins(10, 3, 10, 3);
            layout->setSpacing(0);
            auto *combo = new QComboBox(editor);
            combo->setObjectName("ObjectLayerCombo");
            combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            combo->setMinimumContentsLength(6);
            combo->setPlaceholderText(tr("Разные значения"));
            combo->setEnabled(std::none_of(m_currentObjects.cbegin(), m_currentObjects.cend(), [](const auto &object) {
                return dynamic_cast<SymmetryCopyObject *>(object.data()) != nullptr;
            }));
            layout->addWidget(combo);
            m_tableWidget->setCellWidget(i, 1, editor);
            connect(combo, &QComboBox::activated, this, [this, combo](int index) {
                const auto objects = m_currentObjects;
                ProjectManager::instance()->setObjectsLayer(objects, combo->itemData(index).toInt());
                emit propertyChanged();
            });
        } else if (props[i].first == QStringLiteral("Прозрачность")) {
            auto *editor = createTransparencyEditor(m_mixedProperties.contains(props[i].first) ? -1 : props[i].second.toDouble());
            m_tableWidget->setCellWidget(i, 1, editor);
        }
        updateValueItem(i, props[i].first, props[i].second);
    }
    resizePropertyRows();
    m_updating = false;
}

void ObjectPropertiesPanel::updateValueItem(int row, const QString &name, const QString &value)
{
    auto *item = m_tableWidget->item(row, 1);
    const bool mixed = m_mixedProperties.contains(name);
    item->setText(value);
    item->setData(Qt::UserRole, mixed);
    item->setData(Qt::UserRole + 1, value);
    item->setIcon(QIcon());
    const bool readOnly = isReadOnlyProperty(name);
    const bool numeric = !m_currentObjects.isEmpty() && std::all_of(m_currentObjects.cbegin(), m_currentObjects.cend(), [&name](const auto &object) {
        return object->isNumericProperty(name);
    });
    item->setToolTip(!readOnly && numeric ? NumericExpression::helpText() : value);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    if (!readOnly && !isColorProperty(name) && name != tr("Слой") && name != QStringLiteral("Прозрачность"))
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    item->setForeground(readOnly || mixed ? QBrush(QColor("#9fb3c1")) : QBrush());
    if (name == tr("Слой")) {
        auto *combo = m_tableWidget->cellWidget(row, 1)->findChild<QComboBox *>("ObjectLayerCombo");
        const QSignalBlocker blocker(combo);
        const auto &layers = ProjectManager::instance()->editorLayers();
        bool rebuild = combo->count() != layers.size();
        for (int i = 0; !rebuild && i < layers.size(); ++i)
            rebuild = combo->itemData(i).toInt() != layers[i].id || combo->itemText(i) != layers[i].name;
        if (rebuild) {
            combo->clear();
            for (const auto &layer : layers) combo->addItem(layer.name, layer.id);
        }
        combo->setCurrentIndex(mixed ? -1 : combo->findData(m_currentObjects.first()->editorLayerId()));
        item->setText({});
    } else if (name == QStringLiteral("Прозрачность")) {
        auto *editor = m_tableWidget->cellWidget(row, 1);
        auto *spin = editor->findChild<QSpinBox *>("TransparencySpin");
        auto *slider = editor->findChild<QSlider *>("TransparencySlider");
        const QSignalBlocker spinBlocker(spin), sliderBlocker(slider);
        spin->setSpecialValueText(mixed ? tr("Разные") : QString());
        spin->setMinimum(mixed ? -1 : 0);
        spin->setValue(mixed ? -1 : qRound(value.toDouble()));
        slider->setValue(mixed ? 0 : qRound(value.toDouble()));
        item->setText({});
    } else if (isColorProperty(name) && !mixed) {
        const QColor color(value);
        if (color.isValid()) {
            QPixmap pixmap(16, 16); pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(color); painter.setPen(QColor("#334656"));
            painter.drawEllipse(1, 1, 14, 14); painter.end();
            item->setIcon(QIcon(pixmap));
        }
    }
    if (auto *editor = m_tableWidget->cellWidget(row, 0); editor && editor->objectName() == QStringLiteral("StaticLayerEditor")) {
        auto *combo = editor->findChild<QComboBox *>("StaticLayerOrderCombo");
        const QSignalBlocker blocker(combo);
        int count = std::numeric_limits<int>::max();
        for (const auto &object : m_currentObjects) {
            const auto *image = dynamic_cast<const ImageObject *>(object.data());
            const int blocks = image ? image->colorLayers().size() : static_cast<const StaticGroupObject *>(object.data())->states.size();
            count = qMin(count, blocks);
        }
        if (combo->count() != count) {
            combo->clear();
            for (int position = 1; position <= count; ++position) combo->addItem(QString::number(position), position);
        }
        const auto orderProperty = layerOrderProperty(name);
        int position = -1;
        if (!m_mixedProperties.contains(orderProperty))
            for (const auto &property : m_currentObjects.first()->getProperties())
                if (property.first == orderProperty) { position = property.second.toInt() - 1; break; }
        combo->setCurrentIndex(position);
        item->setText(mixed ? tr("Разные цвета") : tr("Цвет"));
        item->setToolTip(mixed ? tr("Разные значения") : value);
    }
}

void ObjectPropertiesPanel::clearTable()
{
    if (m_transparencyDragging) {
        m_transparencyDragging = false;
        QTimer::singleShot(0, ProjectManager::instance(), []() { ProjectManager::instance()->finishObjectEdit(); });
    }
    m_updating = true;
    m_tableWidget->clearContents();
    m_tableWidget->setRowCount(0);
    m_updating = false;
}

QWidget *ObjectPropertiesPanel::createTransparencyEditor(double value)
{
    auto *editor = new QWidget(m_tableWidget);
    editor->setObjectName("TransparencyEditor");
    auto *layout = new QHBoxLayout(editor);
    layout->setContentsMargins(10, 3, 10, 3);
    layout->setSpacing(8);
    auto *spin = new ExpressionSpinBox(editor);
    spin->setObjectName("TransparencySpin");
    spin->setRange(value < 0 ? -1 : 0, 100);
    spin->setSpecialValueText(value < 0 ? tr("Разные") : QString());
    spin->setSingleStep(1);
    spin->setSuffix(QStringLiteral(" %")); spin->setKeyboardTracking(false);
    spin->setMinimumWidth(78);
    spin->setValue(value);
    auto *slider = new QSlider(Qt::Horizontal, editor);
    slider->setObjectName("TransparencySlider");
    slider->setRange(0, 100); slider->setSingleStep(1); slider->setPageStep(10);
    slider->setMinimumWidth(24); slider->setValue(qRound(value));
    const QString hint = tr("0% — непрозрачно, 100% — полностью прозрачно. На ПЛИС промежуточные значения округляются до 8 аппаратных уровней.");
    spin->setToolTip(hint); slider->setToolTip(hint);
    layout->addWidget(spin); layout->addWidget(slider, 1);
    connect(spin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_updating && value >= 0) applyTransparency(value, false);
    });
    connect(slider, &QSlider::sliderPressed, this, [this]() {
        ProjectManager::instance()->recordObjectEdit();
        m_transparencyDragging = true;
    });
    connect(slider, &QSlider::valueChanged, this, [this, spin](int value) {
        const QSignalBlocker blocker(spin);
        spin->setValue(value);
        if (!m_updating) applyTransparency(value, m_transparencyDragging);
    });
    connect(slider, &QSlider::sliderReleased, this, [this, slider]() {
        if (!m_transparencyDragging) return;
        applyTransparency(slider->value(), true);
        m_transparencyDragging = false;
        ProjectManager::instance()->finishObjectEdit();
        refreshTableValues();
    });
    return editor;
}

void ObjectPropertiesPanel::resizePropertyRows()
{
    QStyleOptionViewItem option;
    option.initFrom(m_tableWidget);
    option.widget = m_tableWidget;
    int editorHeight = m_tableWidget->verticalHeader()->defaultSectionSize();
    for (int row = 0; row < m_tableWidget->rowCount(); ++row) {
        // Высота зависит от подписи и полей ввода, а не от длины значения.
        const auto index = m_tableWidget->model()->index(row, 0);
        editorHeight = qMax(editorHeight, m_tableWidget->itemDelegateForColumn(0)->sizeHint(option, index).height());
        for (int column = 0; column < m_tableWidget->columnCount(); ++column)
            if (auto *editor = m_tableWidget->cellWidget(row, column))
                editorHeight = qMax(editorHeight, editor->sizeHint().height());
    }
    for (int row = 0; row < m_tableWidget->rowCount(); ++row)
        m_tableWidget->setRowHeight(row, editorHeight);
}

void ObjectPropertiesPanel::applyTransparency(double value, bool interactive)
{
    if (m_currentObjects.isEmpty()) return;
    const QString percent = QString::number(qRound(value));
    if (interactive) {
        const auto objects = m_currentObjects;
        for (const auto &object : objects) object->setObjectProperty(QStringLiteral("Прозрачность"), percent);
        emit propertyChanged();
    } else {
        applyProperty(QStringLiteral("Прозрачность"), percent);
    }
}

bool ObjectPropertiesPanel::applyProperty(const QString &name, const QString &value)
{
    const auto objects = m_currentObjects;
    const bool success = ProjectManager::instance()->setObjectsProperty(objects, name, value);
    if (!success && !ProjectManager::instance()->lastErrorMessage().isEmpty())
        QMessageBox::warning(this, tr("Ограничение редактирования"), ProjectManager::instance()->lastErrorMessage());
    refreshTableValues();
    if (success) emit propertyChanged();
    return success;
}

void ObjectPropertiesPanel::focusTransparency()
{
    if (auto *spin = m_tableWidget->findChild<QSpinBox *>("TransparencySpin")) {
        m_scrollArea->ensureWidgetVisible(spin);
        spin->setFocus(); spin->selectAll();
    }
}

void ObjectPropertiesPanel::refreshHardwareWarnings()
{
    QStringList messages;
    bool errors = false;
    for (const auto &object : m_currentObjects) {
        if (dynamic_cast<SymmetryCopyObject *>(object.data()) && !messages.contains(object->editRestrictionHint()))
            messages.append(object->editRestrictionHint());
        const auto report = FpgaResourceLimits::inspect(*object);
        errors |= !report.errors.isEmpty();
        for (const auto &message : report.messages()) if (!messages.contains(message)) messages.append(message);
    }
    m_subtitleLabel->setText(messages.join('\n'));
    m_subtitleLabel->setProperty("severity", errors ? "error" : "warning");
    m_subtitleLabel->style()->unpolish(m_subtitleLabel);
    m_subtitleLabel->style()->polish(m_subtitleLabel);
    m_subtitleLabel->setVisible(!messages.isEmpty());
}

void ObjectPropertiesPanel::onCellChanged(int row, int column)
{
    if (m_updating || column != 1 || m_currentObjects.isEmpty()) return;
    auto *name = m_tableWidget->item(row, 0);
    auto *value = m_tableWidget->item(row, 1);
    if (name && value && !isReadOnlyProperty(name->text())) applyProperty(name->text(), value->text());
}

void ObjectPropertiesPanel::onCellDoubleClicked(int row, int column)
{
    if (column != 1 || !m_currentObject)
        return;

    auto *nameItem = m_tableWidget->item(row, 0);
    auto *valueItem = m_tableWidget->item(row, 1);
    if (!nameItem || !valueItem)
        return;

    const QString propertyName = nameItem->text();
    if (!isColorProperty(propertyName) || isReadOnlyProperty(propertyName))
        return;

    const QColor currentColor(valueItem->data(Qt::UserRole + 1).toString());
    const QColor selectedColor = QColorDialog::getColor(currentColor, this, tr("Выбор цвета"), QColorDialog::DontUseNativeDialog);
    if (!selectedColor.isValid())
        return;

    applyProperty(propertyName, selectedColor.name());
}

void ObjectPropertiesPanel::refreshTableValues()
{
    if (m_currentObjects.isEmpty()) return;
    const auto props = commonProperties();
    bool rebuild = props.size() != m_tableWidget->rowCount();
    for (int i = 0; !rebuild && i < props.size(); ++i)
        rebuild = !m_tableWidget->item(i, 0) || m_tableWidget->item(i, 0)->text() != props[i].first;
    if (rebuild) {
        clearTable();
        populateTable(props);
    } else {
        m_updating = true;
        for (int i = 0; i < props.size(); ++i) updateValueItem(i, props[i].first, props[i].second);
        resizePropertyRows();
        m_updating = false;
    }
    refreshHardwareWarnings();
}

bool ObjectPropertiesPanel::isColorProperty(const QString &name) const
{
    return name.contains(QStringLiteral("Цвет"))
        || name == QStringLiteral("Заливка")
        || name == QStringLiteral("Обводка");
}

bool ObjectPropertiesPanel::isReadOnlyProperty(const QString &name) const
{
    if (name == tr("Слой") && std::any_of(m_currentObjects.cbegin(), m_currentObjects.cend(), [](const auto &object) {
        return dynamic_cast<SymmetryCopyObject *>(object.data()) != nullptr;
    })) return true;
    return m_readOnlyProperties.contains(name);
}
