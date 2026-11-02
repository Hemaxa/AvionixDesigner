//ObjectPropertiesPanel - панель свойств выбранного объекта

#include "ObjectPropertiesPanel.h"

#include "BaseObject.h"
#include "ProjectManager.h"
#include "ThreeDViewObject.h"
#include "fpga/model/FpgaResourceLimits.h"

#include <QColorDialog>
#include <QComboBox>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
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
// Выбирает редактор значения свойства: список для положения, шрифта и флагов.
class PropertyValueDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (editor->objectName() == QStringLiteral("TransparencyEditor")) {
            // У составного поля свои отступы: используем всю высоту обычной строки,
            // без повторного вычитания padding из стиля ячейки.
            editor->setGeometry(option.rect);
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
            combo->addItems({QStringLiteral("Справа"), QStringLiteral("Слева")});
            return combo;
        }
        if (propertyName == QStringLiteral("Шрифт")) {
            auto *combo = new QComboBox(parent);
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
            QStringLiteral("Видимость")
        };

        if (boolNames.contains(propertyName) || propertyName.endsWith(QStringLiteral("Видимость"))) {
            auto *combo = new QComboBox(parent);
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
                combo->setCurrentIndex(current == QStringLiteral("нет") ? 1 : 0);
            }
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox*>(editor)) {
            model->setData(index, combo->currentText());
            return;
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
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setAlternatingRowColors(false);
    m_tableWidget->setShowGrid(false);
    m_tableWidget->setWordWrap(true);
    m_tableWidget->setCornerButtonEnabled(false);
    m_tableWidget->setItemDelegateForColumn(1, new PropertyValueDelegate(m_tableWidget));
    tableLayout->addWidget(m_tableWidget);

    m_contentStack->addWidget(tableContainer);
    m_contentStack->setCurrentIndex(0);
    m_titleLabel->hide();

    m_scrollArea->setWidget(container);
    mainLayout->addWidget(m_scrollArea);

    connect(m_tableWidget, &QTableWidget::cellChanged, this, &ObjectPropertiesPanel::onCellChanged);
    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, &ObjectPropertiesPanel::onCellDoubleClicked);
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
    disconnect(m_objectConnection);
    clearTable();
    m_currentObject = obj;

    if (!obj) {
        clearProperties();
        return;
    }

    m_titleLabel->setText(obj->getDisplayName());
    m_titleLabel->show();
    m_subtitleLabel->hide();
    populateTable(obj->getProperties());
    refreshHardwareWarnings();
    m_objectConnection = connect(obj.data(), &BaseObject::changed, this, &ObjectPropertiesPanel::refreshHardwareWarnings);
    m_contentStack->setCurrentIndex(1);
}

void ObjectPropertiesPanel::clearProperties()
{
    disconnect(m_objectConnection);
    clearTable();
    m_currentObject.reset();
    m_titleLabel->clear();
    m_titleLabel->hide();
    m_subtitleLabel->clear();
    m_subtitleLabel->hide();
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
        if (isReadOnlyProperty(props[i].first)) {
            valueItem->setFlags((valueItem->flags() & ~Qt::ItemIsEditable) | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            valueItem->setForeground(QBrush(QColor("#9DB3C3")));
        } else if (isColorProperty(props[i].first)) {
            valueItem->setFlags((valueItem->flags() & ~Qt::ItemIsEditable) | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            const QColor previewColor(props[i].second);
            if (previewColor.isValid()) {
                QPixmap pixmap(16, 16);
                pixmap.fill(Qt::transparent);
                QPainter p(&pixmap);
                p.setRenderHint(QPainter::Antialiasing);
                p.setBrush(previewColor);
                p.setPen(QPen(Qt::gray, 1));
                p.drawEllipse(1, 1, 14, 14);
                p.end();
                valueItem->setIcon(QIcon(pixmap));
            }
        }
        m_tableWidget->setItem(i, 1, valueItem);
        if (props[i].first == QStringLiteral("Прозрачность")) {
            valueItem->setText(QString());
            valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
            m_tableWidget->setCellWidget(i, 1, createTransparencyEditor(props[i].second.toDouble()));
        }
    }

    resizePropertyRows();
    m_updating = false;
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
    auto *spin = new QSpinBox(editor);
    spin->setObjectName("TransparencySpin");
    spin->setRange(0, 100); spin->setSingleStep(1);
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
        if (!m_updating) applyTransparency(value, false);
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
    connect(slider, &QSlider::sliderReleased, this, [this]() {
        if (!m_transparencyDragging) return;
        m_transparencyDragging = false;
        ProjectManager::instance()->finishObjectEdit();
        refreshTableValues();
    });
    return editor;
}

void ObjectPropertiesPanel::resizePropertyRows()
{
    m_tableWidget->resizeRowsToContents();
}

void ObjectPropertiesPanel::applyTransparency(double value, bool interactive)
{
    if (!m_currentObject) return;
    const QString percent = QString::number(qRound(value));
    const bool changed = interactive
        ? m_currentObject->setObjectProperty(QStringLiteral("Прозрачность"), percent)
        : ProjectManager::instance()->setObjectProperty(m_currentObject.data(), QStringLiteral("Прозрачность"), percent);
    if (!changed) return;
    if (!interactive) refreshTableValues();
    emit propertyChanged();
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
    const auto report = m_currentObject ? FpgaResourceLimits::inspect(*m_currentObject) : FpgaResourceLimits::Report{};
    m_subtitleLabel->setText(report.messages().join('\n'));
    m_subtitleLabel->setProperty("severity", report.errors.isEmpty() ? "warning" : "error");
    m_subtitleLabel->style()->unpolish(m_subtitleLabel);
    m_subtitleLabel->style()->polish(m_subtitleLabel);
    m_subtitleLabel->setVisible(!report.messages().isEmpty());
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

    if (ProjectManager::instance()->setObjectProperty(m_currentObject.data(), propName, newValue)) {
        refreshTableValues();
        emit propertyChanged();
    }
    else {
        const QString message = m_currentObject->lastValidationMessage();
        if (!message.isEmpty()) {
            QMessageBox::warning(this, tr("Ограничение редактирования"), message);
            m_currentObject->clearValidationMessage();
        }
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

void ObjectPropertiesPanel::onCellDoubleClicked(int row, int column)
{
    if (column != 1 || !m_currentObject)
        return;

    auto *nameItem = m_tableWidget->item(row, 0);
    auto *valueItem = m_tableWidget->item(row, 1);
    if (!nameItem || !valueItem)
        return;

    const QString propertyName = nameItem->text();
    if (!isColorProperty(propertyName))
        return;

    const QColor currentColor(valueItem->text());
    const QColor selectedColor = QColorDialog::getColor(currentColor, this, tr("Выбор цвета"), QColorDialog::DontUseNativeDialog);
    if (!selectedColor.isValid())
        return;

    if (ProjectManager::instance()->setObjectProperty(m_currentObject.data(), propertyName, selectedColor.name())) {
        refreshTableValues();
        emit propertyChanged();
    }
}

void ObjectPropertiesPanel::refreshTableValues()
{
    if (!m_currentObject)
        return;

    m_updating = true;
    const auto props = m_currentObject->getProperties();
    for (int i = 0; i < props.size(); ++i) {
        if (i >= m_tableWidget->rowCount())
            break;

        if (auto *name = m_tableWidget->item(i, 0)) {
            name->setText(props[i].first);
        }
        if (auto *value = m_tableWidget->item(i, 1)) {
            value->setText(props[i].first == QStringLiteral("Прозрачность") ? QString() : props[i].second);
            if (props[i].first == QStringLiteral("Прозрачность")) {
                if (auto *editor = m_tableWidget->cellWidget(i, 1)) {
                    auto *spin = editor->findChild<QSpinBox *>("TransparencySpin");
                    auto *slider = editor->findChild<QSlider *>("TransparencySlider");
                    const QSignalBlocker spinBlocker(spin), sliderBlocker(slider);
                    spin->setValue(props[i].second.toDouble());
                    slider->setValue(qRound(props[i].second.toDouble()));
                }
            }
            if (isReadOnlyProperty(props[i].first)) {
                value->setFlags((value->flags() & ~Qt::ItemIsEditable) | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                value->setIcon(QIcon());
                value->setBackground(QBrush());
                value->setForeground(QBrush(QColor("#9DB3C3")));
            } else if (isColorProperty(props[i].first)) {
                value->setFlags((value->flags() & ~Qt::ItemIsEditable) | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                const QColor previewColor(props[i].second);
                if (previewColor.isValid()) {
                    QPixmap pixmap(16, 16);
                    pixmap.fill(Qt::transparent);
                    QPainter p(&pixmap);
                    p.setRenderHint(QPainter::Antialiasing);
                    p.setBrush(previewColor);
                    p.setPen(QPen(Qt::gray, 1));
                    p.drawEllipse(1, 1, 14, 14);
                    p.end();
                    value->setIcon(QIcon(pixmap));
                    value->setBackground(QBrush());
                    value->setForeground(QBrush());
                }
            }
        }
    }
    resizePropertyRows();
    m_updating = false;
    refreshHardwareWarnings();
}

bool ObjectPropertiesPanel::isColorProperty(const QString &name) const
{
    return name.contains(QStringLiteral("Цвет"))
        || name == QStringLiteral("Заливка")
        || name == QStringLiteral("Обводка");
}

bool ObjectPropertiesPanel::isBooleanProperty(const QString &name) const
{
    return name == QStringLiteral("Включен")
        || name.endsWith(QStringLiteral("Видимость"));
}

bool ObjectPropertiesPanel::isReadOnlyProperty(const QString &name) const
{
    if (dynamic_cast<ThreeDViewObject*>(m_currentObject.data()))
        return name != QStringLiteral("Положение");
    return name == QStringLiteral("XML экспорт") || name == QStringLiteral("Память")
        || name == QStringLiteral("Кол-во состояний");
}
