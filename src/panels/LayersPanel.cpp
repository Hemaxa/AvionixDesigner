// LayersPanel - панель выбора и управления редакторскими слоями проекта.

#include "LayersPanel.h"
#include "ProjectManager.h"

#include <QApplication>
#include <QAction>
#include <QColorDialog>
#include <QEvent>
#include <QHBoxLayout>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

LayersPanel::LayersPanel(QWidget *parent) : BasePanel(parent)
{
    setPanelName("LayersPanel");
    setMinimumSize(280, 220);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    m_allLayers = new QToolButton(this);
    m_allLayers->setObjectName("ShowAllLayersButton");
    m_allLayers->setText(tr("Все слои"));
    m_allLayers->setToolTip(tr("Показать объекты всех слоёв (Ctrl+Shift+A). Новые объекты добавляются на выбранный слой."));
    m_allLayers->setCheckable(true);
    m_allLayers->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(m_allLayers);
    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName("LayersScrollArea");
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_rows = new QWidget();
    m_rows->setObjectName("LayersRows");
    m_rowsLayout = new QVBoxLayout(m_rows);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(6);
    m_scroll->setWidget(m_rows);
    layout->addWidget(m_scroll, 1);
    auto *add = new QToolButton(this);
    add->setObjectName("AddLayerButton");
    add->setFixedSize(32, 32);
    add->setIconSize(QSize(18, 18));
    add->setToolTip(tr("Добавить слой (Ctrl+Shift+N)"));
    add->setAccessibleName(tr("Добавить слой"));
    add->setIcon(QIcon(QStringLiteral(":/icons/icons/layers/add.svg")));
    layout->addWidget(add, 0, Qt::AlignHCenter);
    auto *project = ProjectManager::instance();
    connect(add, &QToolButton::clicked, project, &ProjectManager::addEditorLayer);
    connect(m_allLayers, &QToolButton::toggled, project, &ProjectManager::setShowAllLayers);
    // RowCommand - команда активного слоя; сочетание создаётся один раз на панель.
    struct RowCommand { const char *button; const char *shortcut; const char *title; };
    for (const auto &command : {
             RowCommand{"AddLayerButton", "Ctrl+Shift+N", "Добавить слой"},
             RowCommand{"ShowAllLayersButton", "Ctrl+Shift+A", "Показать все слои"},
             RowCommand{"DuplicateLayerButton", "Ctrl+Shift+D", "Дублировать слой"},
             RowCommand{"MergeLayerButton", "Ctrl+Shift+M", "Объединить выделенные слои"},
             RowCommand{"DeleteLayerButton", "Ctrl+Shift+Delete", "Удалить слой"},
             RowCommand{"LayerColorButton", "Ctrl+Shift+C", "Цвет выделения слоя"}}) {
        auto *action = new QAction(tr(command.title), this);
        action->setShortcut(QKeySequence(command.shortcut));
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        addAction(action);
        connect(action, &QAction::triggered, this,
            [this, name = QString::fromLatin1(command.button)] { activateLayerButton(name); });
    }
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    connect(m_refreshTimer, &QTimer::timeout, this, &LayersPanel::refreshLayers);
    connect(project, &ProjectManager::editorLayersChanged, this, [this]() { m_refreshTimer->start(0); });
    connect(project, &ProjectManager::projectChanged, this, [this, project]() {
        if (!project->isAircraftStateChange()) m_refreshTimer->start(0);
    });
    connect(project, &ProjectManager::projectLoaded, this, [this]() {
        m_selectedLayerIds.clear();
        m_selectionAnchor = -1;
        m_knownActiveLayer = -1;
        refreshLayers();
    });
    connect(project, &ProjectManager::editorLayerViewChanged, this, [this, project]() {
        if (!m_selectingLayer && m_knownActiveLayer != project->activeLayerId()) {
            m_selectedLayerIds = {project->activeLayerId()};
            m_selectionAnchor = project->activeLayerId();
        }
        refreshViewState();
    });
    refreshLayers();
}

QWidget *LayersPanel::activeLayerRow() const
{
    for (int i = 0; i < m_rowsLayout->count(); ++i) {
        auto *row = m_rowsLayout->itemAt(i)->widget();
        if (row && row->property("layerId").toInt() == ProjectManager::instance()->activeLayerId()) return row;
    }
    return nullptr;
}

void LayersPanel::activateLayerButton(const QString &objectName)
{
    if (objectName == "AddLayerButton" || objectName == "ShowAllLayersButton") {
        if (auto *button = findChild<QToolButton *>(objectName)) button->click();
        return;
    }
    if (auto *row = activeLayerRow())
        if (auto *button = row->findChild<QToolButton *>(objectName)) button->click();
}

bool LayersPanel::eventFilter(QObject *watched, QEvent *event)
{
    const auto layerId = watched->property("layerId");
    if (!layerId.isValid()) return BasePanel::eventFilter(watched, event);
    const int id = layerId.toInt();
    auto *name = qobject_cast<QLineEdit *>(watched);
    if (event->type() == QEvent::MouseButtonPress && (!name || name->isReadOnly())) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            selectLayer(id, mouse->modifiers());
            return true;
        }
    }
    if (name && event->type() == QEvent::MouseButtonDblClick) {
        if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
            name->setReadOnly(false);
            name->setCursor(Qt::IBeamCursor);
            name->setFocus();
            name->selectAll();
            return true;
        }
    }
    if (name && event->type() == QEvent::FocusIn && !m_selectingLayer && !m_rebuilding) {
        const auto reason = static_cast<QFocusEvent *>(event)->reason();
        if (reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason) selectLayer(id, {});
    }
    if (name && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (!name->isReadOnly()) {
            if (key->key() == Qt::Key_Escape) {
                name->setText(ProjectManager::instance()->editorLayerName(id));
                name->setReadOnly(true);
                name->setCursor(Qt::ArrowCursor);
                name->deselect();
                return true;
            }
        } else if (key->key() == Qt::Key_F2 || key->key() == Qt::Key_Return) {
            name->setReadOnly(false);
            name->setCursor(Qt::IBeamCursor);
            name->selectAll();
            return true;
        } else if (key->matches(QKeySequence::SelectAll)) {
            m_selectedLayerIds.clear();
            for (const auto &layer : ProjectManager::instance()->editorLayers()) m_selectedLayerIds.insert(layer.id);
            refreshViewState();
            return true;
        } else if (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) {
            const auto &layers = ProjectManager::instance()->editorLayers();
            for (int i = 0; i < layers.size(); ++i) if (layers[i].id == id) {
                const int next = qBound(0, i + (key->key() == Qt::Key_Up ? -1 : 1), int(layers.size()) - 1);
                selectLayer(layers[next].id, key->modifiers());
                return true;
            }
        }
    }
    return BasePanel::eventFilter(watched, event);
}

void LayersPanel::selectLayer(int id, Qt::KeyboardModifiers modifiers)
{
    auto *project = ProjectManager::instance();
    if (!project->hasEditorLayer(id)) return;
    const bool toggle = modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::MetaModifier);
    int active = id;
    if (modifiers.testFlag(Qt::ShiftModifier) && project->hasEditorLayer(m_selectionAnchor)) {
        if (!toggle) m_selectedLayerIds.clear();
        const auto &layers = project->editorLayers();
        int first = 0, last = 0;
        for (int i = 0; i < layers.size(); ++i) {
            if (layers[i].id == m_selectionAnchor) first = i;
            if (layers[i].id == id) last = i;
        }
        for (int i = qMin(first, last); i <= qMax(first, last); ++i) m_selectedLayerIds.insert(layers[i].id);
    } else if (toggle) {
        if (m_selectedLayerIds.contains(id) && m_selectedLayerIds.size() > 1) {
            m_selectedLayerIds.remove(id);
            active = project->activeLayerId();
            if (!m_selectedLayerIds.contains(active)) {
                for (const auto &layer : project->editorLayers())
                    if (m_selectedLayerIds.contains(layer.id)) { active = layer.id; break; }
            }
        } else {
            m_selectedLayerIds.insert(id);
        }
        m_selectionAnchor = id;
    } else {
        m_selectedLayerIds = {id};
        m_selectionAnchor = id;
    }
    m_selectingLayer = true;
    project->setActiveLayer(active);
    for (auto *name : m_rows->findChildren<QLineEdit *>("LayerNameEdit"))
        if (name->property("layerId").toInt() == id) { name->setFocus(Qt::MouseFocusReason); break; }
    m_selectingLayer = false;
    refreshViewState();
}

void LayersPanel::refreshLayers()
{
    const bool restoreFocus = m_rows->isAncestorOf(QApplication::focusWidget());
    m_refreshTimer->stop();
    m_rebuilding = true;
    auto *project = ProjectManager::instance();
    while (auto *item = m_rowsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    int number = 0;
    for (const auto &layer : project->editorLayers()) {
        auto *row = new QWidget(m_rows);
        row->setFixedHeight(44);
        row->setObjectName("LayerRow");
        row->setProperty("layerId", layer.id);
        row->installEventFilter(this);
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(6, 4, 6, 4);
        layout->setSpacing(4);
        auto *color = new QToolButton(row);
        color->setObjectName("LayerColorButton");
        color->setFixedSize(32, 32);
        color->setToolTip(tr("Цвет выделения на слое (Ctrl+Shift+C)"));
        QPixmap swatch(16, 16); swatch.fill(layer.selectionColor);
        color->setIcon(QIcon(swatch));
        layout->addWidget(color);
        auto *order = new QLabel(QString::number(++number), row);
        order->setObjectName("LayerNumber");
        order->setFixedWidth(18);
        order->setProperty("layerId", layer.id);
        order->installEventFilter(this);
        layout->addWidget(order);
        auto *name = new QLineEdit(layer.name, row);
        name->setObjectName("LayerNameEdit");
        name->setProperty("layerId", layer.id);
        name->installEventFilter(this);
        name->setMinimumWidth(70);
        name->setReadOnly(true);
        name->setCursor(Qt::ArrowCursor);
        name->setToolTip(tr("Ctrl/⌘ — несколько слоёв, Shift — диапазон. F2 или двойной щелчок — изменить имя. Объектов: %1").arg(project->layerObjectCount(layer.id)));
        layout->addWidget(name, 1);
        auto action = [&](const QString &icon, const QString &tip, const QString &objectName) {
            auto *button = new QToolButton(row);
            button->setObjectName(objectName);
            button->setIcon(QIcon(QStringLiteral(":/icons/icons/layers/%1.svg").arg(icon)));
            button->setIconSize(QSize(18, 18));
            button->setToolTip(tip);
            button->setAccessibleName(tip);
            button->setFixedSize(32, 32);
            layout->addWidget(button);
            return button;
        };
        auto *duplicate = action(QStringLiteral("duplicate"), tr("Дублировать слой с объектами"), "DuplicateLayerButton");
        auto *merge = action(QStringLiteral("merge"), tr("Объединить выделенные слои в активный слой"), "MergeLayerButton");
        auto *remove = action(QStringLiteral("delete"), tr("Удалить слой с объектами"), "DeleteLayerButton");
        duplicate->setToolTip(tr("Дублировать слой с объектами (Ctrl+Shift+D)"));
        merge->setToolTip(tr("Объединить выделенные слои в активный слой (Ctrl+Shift+M)"));
        remove->setToolTip(tr("Удалить слой с объектами (Ctrl+Shift+Delete)"));
        merge->setEnabled(false);
        remove->setEnabled(project->editorLayers().size() > 1);
        m_rowsLayout->addWidget(row);
        row->show();
        connect(name, &QLineEdit::editingFinished, this, [this, name, id = layer.id]() {
            if (m_rebuilding) return;
            name->setReadOnly(true);
            name->setCursor(Qt::ArrowCursor);
            name->deselect();
            auto *project = ProjectManager::instance();
            if (!project->renameEditorLayer(id, name->text())) {
                name->setText(project->editorLayerName(id));
                name->setToolTip(tr("Имя должно быть непустым и уникальным."));
            }
        });
        connect(color, &QToolButton::clicked, this, [this, id = layer.id]() {
            const QColor color = QColorDialog::getColor(ProjectManager::instance()->editorLayerColor(id), this,
                tr("Цвет выделения слоя"), QColorDialog::DontUseNativeDialog);
            if (color.isValid()) ProjectManager::instance()->setEditorLayerColor(id, color);
        });
        connect(duplicate, &QToolButton::clicked, this, [this, id = layer.id]() {
            auto *project = ProjectManager::instance();
            if (project->duplicateEditorLayer(id) < 0 && !project->lastErrorMessage().isEmpty())
                QMessageBox::information(this, tr("Дублирование слоя"), project->lastErrorMessage());
        });
        connect(merge, &QToolButton::clicked, this, &LayersPanel::mergeSelectedLayers);
        connect(remove, &QToolButton::clicked, this, [this, id = layer.id]() { deleteLayer(id); });
    }
    m_rowsLayout->addStretch();
    m_rowsLayout->activate();
    m_rebuilding = false;
    refreshViewState();
    if (restoreFocus) if (auto *row = activeLayerRow())
        if (auto *name = row->findChild<QLineEdit *>("LayerNameEdit")) name->setFocus(Qt::ShortcutFocusReason);
}

void LayersPanel::refreshViewState()
{
    auto *project = ProjectManager::instance();
    for (auto it = m_selectedLayerIds.begin(); it != m_selectedLayerIds.end(); ) {
        if (!project->hasEditorLayer(*it)) it = m_selectedLayerIds.erase(it);
        else ++it;
    }
    m_selectedLayerIds.insert(project->activeLayerId());
    if (!project->hasEditorLayer(m_selectionAnchor)) m_selectionAnchor = project->activeLayerId();
    m_knownActiveLayer = project->activeLayerId();
    const QSignalBlocker blocker(m_allLayers);
    m_allLayers->setChecked(project->showAllLayers());
    for (int i = 0; i < m_rowsLayout->count(); ++i) {
        auto *row = m_rowsLayout->itemAt(i)->widget();
        if (!row) continue;
        const int id = row->property("layerId").toInt();
        row->setProperty("selected", m_selectedLayerIds.contains(id));
        row->setProperty("active", id == project->activeLayerId());
        if (auto *merge = row->findChild<QToolButton *>("MergeLayerButton"))
            merge->setEnabled(m_selectedLayerIds.size() > 1 && m_selectedLayerIds.contains(id));
        row->style()->unpolish(row); row->style()->polish(row);
        row->update();
    }
}

void LayersPanel::mergeSelectedLayers()
{
    auto *project = ProjectManager::instance();
    project->mergeEditorLayers(m_selectedLayerIds.values(), project->activeLayerId());
}

void LayersPanel::deleteLayer(int id)
{
    auto *project = ProjectManager::instance();
    const int count = project->layerObjectCount(id);
    if (count && QMessageBox::question(this, tr("Удалить слой"),
        tr("Удалить слой «%1» и все его объекты (%2)?").arg(project->editorLayerName(id)).arg(count),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    project->removeEditorLayer(id);
}
