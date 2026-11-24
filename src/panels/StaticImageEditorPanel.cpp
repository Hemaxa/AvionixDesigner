// StaticImageEditorPanel — черновики, навигация, маски и применение к сцене.
#include "StaticImageEditorPanel.h"
#include "ProjectManager.h"
#include "StaticImageCanvas.h"
#include "fpga/compiler/FpgaResourceLimits.h"
#include <QAbstractItemModel>
#include <QApplication>
#include <QColorDialog>
#include <QDrag>
#include <QDropEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMimeData>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
// QListWidget's default InternalMove can treat a drop as replacement on an item.
// Always insert between rows, including the space before the first / after the last row.
class StaticMaskList : public QListWidget
{
  public:
    using QListWidget::QListWidget;
    void moveSelectionTo(int insertion)
    {
        const auto selection = selectedItems();
        if (selection.isEmpty())
            return;
        QList<QListWidgetItem *> moving, remaining;
        int destination = 0;
        for (int i = 0; i < count(); ++i)
            if (selection.contains(item(i)))
                moving.append(item(i));
            else
            {
                if (i < insertion)
                    ++destination;
                remaining.append(item(i));
            }
        for (int i = 0; i < moving.size(); ++i)
            remaining.insert(destination + i, moving[i]);
        for (int i = 0; i < remaining.size(); ++i)
        {
            const int from = row(remaining[i]);
            if (from != i)
                model()->moveRow({}, from, {}, i);
        }
    }

  protected:
    void startDrag(Qt::DropActions) override
    {
        QDrag drag(this);
        drag.setMimeData(model()->mimeData(selectedIndexes()));
        if (currentItem())
        {
            const auto preview = visualItemRect(currentItem()).intersected(viewport()->rect());
            if (!preview.isEmpty())
            {
                drag.setPixmap(viewport()->grab(preview));
                drag.setHotSpot(QPoint(24, preview.height() / 2));
            }
        }
        drag.exec(Qt::MoveAction);
        m_dropRow = -1;
        setState(QAbstractItemView::NoState);
        viewport()->update();
    }
    void dragMoveEvent(QDragMoveEvent *event) override
    {
        QListWidget::dragMoveEvent(event);
        if (event->source() != this)
            return;
        m_dropRow = insertionAt(event->position().toPoint());
        event->setDropAction(Qt::MoveAction);
        event->accept();
        viewport()->update();
    }
    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        m_dropRow = -1;
        QListWidget::dragLeaveEvent(event);
        viewport()->update();
    }
    void dropEvent(QDropEvent *event) override
    {
        if (event->source() != this)
        {
            event->ignore();
            return;
        }
        moveSelectionTo(insertionAt(event->position().toPoint()));
        m_dropRow = -1;
        event->setDropAction(Qt::MoveAction);
        event->accept();
        viewport()->update();
    }
    void paintEvent(QPaintEvent *event) override
    {
        QListWidget::paintEvent(event);
        if (m_dropRow < 0)
            return;
        const int y = m_dropRow < count() ? visualItemRect(item(m_dropRow)).top()
                                          : (count() ? visualItemRect(item(count() - 1)).bottom() : 2);
        QPainter painter(viewport());
        painter.setPen(QPen(QColor("#00c8f0"), 2));
        painter.drawLine(8, y, viewport()->width() - 8, y);
    }

  private:
    int insertionAt(const QPoint &point) const
    {
        for (int i = 0; i < count(); ++i)
            if (point.y() < visualItemRect(item(i)).center().y())
                return i;
        return count();
    }
    int m_dropRow = -1;
};

// StaticMaskDelegate — одинаковые цветовые образцы и две выровненные строки описания маски.
class StaticMaskDelegate : public QStyledItemDelegate
{
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem background(option);
        initStyleOption(&background, index);
        const auto lines = background.text.split('\n');
        background.text.clear();
        background.icon = {};
        const auto *style = option.widget ? option.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &background, painter, option.widget);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF swatch(option.rect.left() + 12, option.rect.center().y() - 11, 22, 22);
        painter->setPen(QPen(QColor("#9fb3c1"), 1));
        painter->setBrush(index.data(Qt::UserRole + 1).value<QColor>());
        painter->drawRoundedRect(swatch, 4, 4);
        const auto text = option.rect.adjusted(46, 9, -10, -9);
        painter->setPen(QColor("#edf7fb"));
        painter->drawText(text, Qt::AlignLeft | Qt::AlignTop,
                          option.fontMetrics.elidedText(lines.value(0), Qt::ElideRight, text.width()));
        painter->setPen(QColor("#9fb3c1"));
        painter->drawText(text, Qt::AlignLeft | Qt::AlignBottom,
                          option.fontMetrics.elidedText(lines.value(1), Qt::ElideRight, text.width()));
        painter->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(240, 60);
    }
};
} // namespace

StaticImageEditorPanel::StaticImageEditorPanel(QWidget *parent) : BasePanel(parent)
{
    setPanelName("StaticImageEditorPanel");
    setMinimumSize(680, 460);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    m_name = new QLabel(tr("Статическое изображение"), this);
    m_name->setObjectName("StaticEditorTitle");
    m_name->setTextFormat(Qt::PlainText);
    m_name->setWordWrap(true);
    layout->addWidget(m_name);
    m_pages = new QStackedWidget(this);
    layout->addWidget(m_pages, 1);
    m_overview = new QWidget(m_pages);
    m_detail = new QWidget(m_pages);
    m_pages->addWidget(m_overview);
    m_pages->addWidget(m_detail);
    auto *overview = new QHBoxLayout(m_overview);
    overview->setContentsMargins(0, 0, 0, 0);
    overview->setSpacing(10);
    m_canvas = new StaticImageCanvas(m_overview);
    m_canvas->setObjectName("StaticPartitionCanvas");
    overview->addWidget(m_canvas, 1);
    // Common tools stay in the same place when entering a region.
    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);
    layout->insertLayout(1, toolbar);
    auto tool =
        [&](QHBoxLayout *bar, QWidget *parent, const char *name, const QString &icon, const QString &tip)
    {
        auto *button = new QToolButton(parent);
        button->setObjectName(name);
        button->setIcon(QIcon(icon));
        button->setIconSize(QSize(22, 22));
        button->setFixedSize(36, 36);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        bar->addWidget(button);
        return button;
    };
    m_mergeTool = tool(toolbar, this, "StaticShapeBuilder", ":/icons/icons/static-editor/shape-builder.svg",
                       tr("Объединение: проведите по областям или маскам"));
    m_mergeTool->setCheckable(true);
    auto *partTool =
        tool(toolbar, this, "StaticMaskShapeBuilder", ":/icons/icons/static-editor/shape-builder.svg",
             tr("Объединение масок: отпустите мышь на цвете результата"));
    partTool->setCheckable(true);
    partTool->hide();
    m_empty = tool(toolbar, this, "StaticExcludeEmpty", ":/icons/icons/static-editor/exclude-empty.svg",
                   tr("Автоматически исключить области, в которых все значения масок равны 0"));
    toolbar->addSpacing(8);
    m_undo = tool(toolbar, this, "StaticDraftUndo", ":/icons/icons/selection/undo.svg",
                  tr("Отменить изменение в окне"));
    m_redo = tool(toolbar, this, "StaticDraftRedo", ":/icons/icons/selection/redo.svg",
                  tr("Повторить изменение в окне"));
    toolbar->addStretch();
    auto *fit =
        tool(toolbar, this, "StaticFit", ":/icons/icons/static-editor/fit.svg", tr("Вписать изображение"));

    auto *sideScroll = new QScrollArea(m_overview);
    sideScroll->setObjectName("StaticStatsScroll");
    sideScroll->setWidgetResizable(true);
    sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideScroll->setMinimumWidth(258);
    sideScroll->setMaximumWidth(320);
    sideScroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    overview->addWidget(sideScroll);
    auto *side = new QWidget;
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(10);
    sideScroll->setWidget(side);
    auto *statsTitle = new QLabel(tr("Данные изображения"));
    statsTitle->setObjectName("StaticSectionTitle");
    sideLayout->addWidget(statsTitle);
    m_statsEmpty = new QLabel(tr("Выберите один статический объект."));
    m_statsEmpty->setWordWrap(true);
    sideLayout->addWidget(m_statsEmpty);
    m_stats = new QFrame;
    m_stats->setObjectName("StaticEditorStats");
    sideLayout->addWidget(m_stats);
    auto *statsGrid = new QGridLayout(m_stats);
    statsGrid->setContentsMargins(12, 12, 12, 12);
    statsGrid->setHorizontalSpacing(12);
    statsGrid->setVerticalSpacing(8);
    statsGrid->setColumnStretch(0, 1);
    int row = 0;
    auto section = [&](const QString &title)
    {
        auto *label = new QLabel(title);
        label->setObjectName("StaticSectionTitle");
        if (row)
            statsGrid->setRowMinimumHeight(row++, 8);
        statsGrid->addWidget(label, row++, 0, 1, 2);
    };
    auto stat = [&](const QString &key, const QString &title, const QString &tip = QString())
    {
        auto *caption = new QLabel(title), *value = new QLabel;
        caption->setObjectName("StaticStatCaption");
        value->setObjectName("StaticStatValue");
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        caption->setToolTip(tip);
        value->setToolTip(tip);
        statsGrid->addWidget(caption, row, 0);
        statsGrid->addWidget(value, row++, 1);
        m_statValues.insert(key, value);
    };
    section(tr("Изображение"));
    stat("size", tr("Размер, px"));
    stat("regions", tr("Всего областей"));
    stat("excluded", tr("Исключено"));
    section(tr("Память масок"));
    stat("before", tr("До, ячеек"));
    stat("after", tr("После, ячеек"));
    stat("saved", tr("Экономия, ячеек"));
    stat("percent", tr("Экономия, %"));
    section(tr("Экспорт в ПЛИС"));
    stat("groups", tr("STATICGROUP"));
    stat("init", tr("Блоков init"));
    stat("bits", tr("Маски, бит"), tr("Одна ячейка маски занимает 3 бита."));
    stat("uart", tr("UART масок, байт"),
         tr("В протоколе значение каждой ячейки передаётся отдельным байтом."));
    auto *note = new QLabel(
        tr("Одна экспортируемая область — один банк. Объём BRAM зависит от глубины банков в прошивке."));
    note->setWordWrap(true);
    note->setObjectName("StaticEditorNote");
    sideLayout->addWidget(note);
    sideLayout->addStretch();

    auto *detailLayout = new QVBoxLayout(m_detail);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(10);
    auto *navigation = new QHBoxLayout;
    detailLayout->addLayout(navigation);
    auto *back = new QPushButton(QIcon(":/icons/icons/static-editor/back.svg"), QString(), m_detail);
    back->setObjectName("StaticPartBack");
    back->setProperty("staticTool", true);
    back->setIconSize(QSize(22, 22));
    back->setFixedSize(36, 36);
    back->setToolTip(tr("К разбиению"));
    back->setAccessibleName(back->toolTip());
    navigation->addWidget(back);
    m_partName = new QLabel(m_detail);
    m_partName->setTextFormat(Qt::PlainText);
    m_partName->setObjectName("StaticSectionTitle");
    m_partName->setWordWrap(true);
    navigation->addWidget(m_partName, 1);
    auto *body = new QSplitter(m_detail);
    body->setChildrenCollapsible(false);
    body->setHandleWidth(8);
    detailLayout->addWidget(body, 1);
    m_partCanvas = new StaticImageCanvas(body);
    m_partCanvas->setObjectName("StaticMaskCanvas");
    auto *maskPanel = new QWidget(body);
    maskPanel->setMinimumWidth(260);
    auto *maskSide = new QVBoxLayout(maskPanel);
    maskSide->setContentsMargins(0, 0, 0, 0);
    maskSide->setSpacing(8);
    auto *maskTitle = new QLabel(tr("Маски части"));
    maskTitle->setObjectName("StaticSectionTitle");
    maskSide->addWidget(maskTitle);
    auto *maskNote = new QLabel(tr("Сверху — первый init, передний план"));
    maskNote->setObjectName("StaticEditorNote");
    maskNote->setWordWrap(true);
    maskNote->setToolTip(tr("Габаритный прямоугольник первой маски перекрывает следующие, в том числе при "
                            "нулевом значении маски."));
    maskSide->addWidget(maskNote);
    m_masks = new StaticMaskList(maskPanel);
    m_masks->setObjectName("StaticMaskList");
    m_masks->setItemDelegate(new StaticMaskDelegate(m_masks));
    m_masks->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_masks->setDragDropMode(QAbstractItemView::InternalMove);
    m_masks->setDragDropOverwriteMode(false);
    m_masks->setDefaultDropAction(Qt::MoveAction);
    m_masks->setDropIndicatorShown(false);
    m_masks->setAutoScrollMargin(28);
    m_masks->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_masks->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    maskSide->addWidget(m_masks, 1);
    auto *maskActions = new QHBoxLayout;
    maskActions->setSpacing(4);
    maskSide->addLayout(maskActions);
    m_top = tool(maskActions, maskPanel, "StaticMaskTop", ":/icons/icons/static-editor/move-top.svg",
                 tr("В самое начало"));
    m_up =
        tool(maskActions, maskPanel, "StaticMaskUp", ":/icons/icons/static-editor/move-up.svg", tr("Выше"));
    m_down = tool(maskActions, maskPanel, "StaticMaskDown", ":/icons/icons/static-editor/move-down.svg",
                  tr("Ниже"));
    m_bottom = tool(maskActions, maskPanel, "StaticMaskBottom", ":/icons/icons/static-editor/move-bottom.svg",
                    tr("В самый конец"));
    maskActions->addStretch();
    auto iconAction = [&](const char *name, const QString &icon, const QString &tip)
    {
        auto *button = new QPushButton(QIcon(icon), QString(), maskPanel);
        button->setObjectName(name);
        button->setProperty("staticTool", true);
        button->setIconSize(QSize(22, 22));
        button->setFixedSize(36, 36);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        maskActions->addWidget(button);
        return button;
    };
    m_color = iconAction("StaticMaskColor", ":/icons/icons/static-editor/color.svg",
                         tr("Изменить цвет выбранных масок"));
    m_merge = iconAction("StaticMergeMasks", ":/icons/icons/static-editor/shape-builder.svg",
                         tr("Объединить выбранные маски"));
    body->setStretchFactor(0, 1);
    body->setStretchFactor(1, 0);
    body->setSizes({650, 280});

    m_hint = new QLabel(this);
    m_hint->setObjectName("StaticEditorHint");
    m_hint->setWordWrap(true);
    m_hint->setMinimumHeight(40);
    layout->addWidget(m_hint);
    auto *footer = new QHBoxLayout;
    layout->addLayout(footer);
    footer->addStretch();
    m_begin = new QPushButton(tr("Начать редактировать"), this);
    m_begin->setObjectName("StaticBeginEditing");
    m_cancel = new QPushButton(tr("Отменить изменения"), this);
    m_cancel->setObjectName("StaticCancelEditing");
    m_apply = new QPushButton(tr("Применить"), this);
    m_apply->setObjectName("StaticApplyEditing");
    m_apply->setProperty("primary", true);
    for (auto *button : {m_begin, m_cancel, m_apply})
        footer->addWidget(button);
    m_transition = new QPropertyAnimation(m_detail, "pos", this);
    m_transition->setDuration(170);
    m_transition->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_transition, &QPropertyAnimation::finished, this,
            [this]
            {
                if (!m_inDetail)
                    m_detail->hide();
            });
    connect(m_begin, &QPushButton::clicked, this, &StaticImageEditorPanel::beginEditing);
    connect(m_cancel, &QPushButton::clicked, this, &StaticImageEditorPanel::cancelEditing);
    connect(m_apply, &QPushButton::clicked, this, [this] { applyEditing(); });
    connect(back, &QPushButton::clicked, this, &StaticImageEditorPanel::showOverview);
    connect(fit, &QToolButton::clicked, this, [this] { (m_inDetail ? m_partCanvas : m_canvas)->fit(); });
    connect(m_mergeTool, &QToolButton::toggled, this,
            [this, partTool](bool checked)
            {
                m_canvas->setMergeTool(checked);
                m_partCanvas->setMergeTool(checked);
                QSignalBlocker blocker(partTool);
                partTool->setChecked(checked);
            });
    connect(partTool, &QToolButton::toggled, m_mergeTool, &QToolButton::setChecked);
    connect(m_undo, &QToolButton::clicked, this, [this] { undoDraft(); });
    connect(m_redo, &QToolButton::clicked, this, [this] { undoDraft(true); });
    connect(m_empty, &QToolButton::clicked, this,
            [this]
            {
                if (!editing())
                    return;
                const bool hasEmpty =
                    std::any_of(m_session->document.regions.cbegin(), m_session->document.regions.cend(),
                                [](const auto &r) { return !r.excluded && r.empty(); });
                if (!hasEmpty)
                {
                    showError(tr("Нет новых областей с полностью нулевыми масками."));
                    return;
                }
                saveHistory();
                m_session->document.excludeEmpty();
                refresh();
            });
    connect(m_canvas, &StaticImageCanvas::splitRequested, this,
            [this](int region, Qt::Orientation orientation, int coordinate)
            {
                if (!editing())
                    return;
                auto next = m_session->document;
                if (!next.split(region, orientation, coordinate))
                    return;
                saveHistory();
                m_session->document = next;
                refresh();
            });
    connect(m_canvas, &StaticImageCanvas::exclusionRequested, this,
            [this](int region)
            {
                if (!editing() || region < 0 || region >= m_session->document.regions.size())
                    return;
                saveHistory();
                auto &part = m_session->document.regions[region];
                part.excluded = !part.excluded;
                refresh();
            });
    connect(m_canvas, &StaticImageCanvas::regionsMergeRequested, this,
            [this](const QList<int> &indexes)
            {
                if (!editing() || indexes.size() < 2)
                    return;
                auto next = m_session->document;
                if (!next.mergeRegions(indexes))
                {
                    showError(tr("Области для объединения должны вместе образовывать прямоугольник."));
                    return;
                }
                saveHistory();
                m_session->document = next;
                refresh();
            });
    connect(m_canvas, &StaticImageCanvas::regionOpened, this, &StaticImageEditorPanel::openRegion);
    connect(m_canvas, &StaticImageCanvas::hintChanged, this,
            [this](const QString &hint)
            {
                if (!m_session || !editing() || !m_session->document.hardwareErrors().isEmpty())
                    return;
                m_hint->setProperty("severity", "normal");
                m_hint->setText(hint);
                m_hint->style()->unpolish(m_hint);
                m_hint->style()->polish(m_hint);
            });
    connect(m_partCanvas, &StaticImageCanvas::masksMergeRequested, this, &StaticImageEditorPanel::mergeMasks);
    connect(m_partCanvas, &StaticImageCanvas::masksSelected, this, &StaticImageEditorPanel::selectMasks);
    connect(m_masks, &QListWidget::itemSelectionChanged, this,
            [this]
            {
                if (m_updating)
                    return;
                m_partCanvas->setSelectedMasks(selectedMasks());
                updateMaskActions();
            });
    connect(m_masks->model(), &QAbstractItemModel::rowsMoved, this, &StaticImageEditorPanel::reorderMasks);
    connect(m_color, &QPushButton::clicked, this, &StaticImageEditorPanel::changeColor);
    connect(m_masks, &QListWidget::itemDoubleClicked, this, [this] { changeColor(); });
    connect(m_merge, &QPushButton::clicked, this,
            [this] { mergeMasks(selectedMasks().values(), m_masks->currentRow()); });
    connect(m_top, &QToolButton::clicked, this, [this] { moveMasks(-1, true); });
    connect(m_up, &QToolButton::clicked, this, [this] { moveMasks(-1); });
    connect(m_down, &QToolButton::clicked, this, [this] { moveMasks(1); });
    connect(m_bottom, &QToolButton::clicked, this, [this] { moveMasks(1, true); });
    connect(m_pages, &QStackedWidget::currentChanged, this,
            [this, partTool]
            {
                const bool detail = m_pages->currentWidget() == m_detail;
                m_mergeTool->setVisible(!detail);
                m_empty->setVisible(!detail);
                partTool->setVisible(detail);
            });
    installEventFilter(this);
    for (auto *widget : findChildren<QWidget *>())
        widget->installEventFilter(this);
    refresh();
}

bool StaticImageEditorPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (editing() && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress))
    {
        auto *key = static_cast<QKeyEvent *>(event);
        const bool maskMove = m_inDetail && (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) &&
                              key->modifiers().testFlag(Qt::AltModifier);
        if (maskMove)
        {
            if (event->type() == QEvent::KeyPress)
                moveMasks(key->key() == Qt::Key_Up ? -1 : 1, key->modifiers().testFlag(Qt::ShiftModifier));
            event->accept();
            return true;
        }
        const bool undo = key->matches(QKeySequence::Undo), redo = key->matches(QKeySequence::Redo);
        if (undo || redo)
        {
            if (event->type() == QEvent::KeyPress)
                undoDraft(redo);
            event->accept();
            return true;
        }
    }
    return BasePanel::eventFilter(watched, event);
}

StaticImageEditorPanel::~StaticImageEditorPanel()
{
    for (const auto &session : m_sessions)
        if (session->editing)
            session->source->setStaticImageEditing(false);
}

bool StaticImageEditorPanel::editing() const
{
    return m_session && m_session->editing;
}
const StaticImageDocument *StaticImageEditorPanel::draft() const
{
    return m_session ? &m_session->document : nullptr;
}

void StaticImageEditorPanel::pruneSessions()
{
    const auto &objects = ProjectManager::instance()->getObjects();
    for (auto it = m_sessions.begin(); it != m_sessions.end();)
    {
        if (!objects.contains(it.value()->source))
        {
            it.value()->source->setStaticImageEditing(false);
            it = m_sessions.erase(it);
        }
        else
            ++it;
    }
}

void StaticImageEditorPanel::setSelectedObjects(const QList<int> &indexes)
{
    if (m_applying)
        return;
    pruneSessions();
    if (!isVisible())
        return;
    auto source = indexes.size() == 1 ? ProjectManager::instance()->getObjectAt(indexes.first())
                                      : QSharedPointer<BaseObject>();
    if (!StaticImageDocument::supports(source.data()))
        source.clear();
    if (m_session && source == m_session->source && m_session->editing)
    {
        m_name->setText(source->customName().isEmpty() ? source->getDisplayName() : source->customName());
        return;
    }
    const bool different = !m_session || m_session->source != source;
    if (different)
    {
        m_transition->stop();
        m_inDetail = false;
        m_pages->setCurrentWidget(m_overview);
        m_detail->hide();
        m_part = -1;
    }
    if (!source)
    {
        m_session.clear();
        refresh();
        return;
    }
    if (!m_sessions.contains(source.data()))
    {
        auto session = QSharedPointer<Session>::create();
        session->source = source;
        m_sessions.insert(source.data(), session);
    }
    m_session = m_sessions.value(source.data());
    if (!m_session->editing)
    {
        QString error;
        m_session->document = StaticImageDocument::fromObject(*source, &error);
        if (!error.isEmpty())
        {
            m_session.clear();
            refresh();
            showError(error);
            return;
        }
    }
    refresh();
}

void StaticImageEditorPanel::beginEditing()
{
    if (!m_session || m_session->editing || m_session->source->isSelectionLocked())
        return;
    m_session->editing = true;
    m_session->source->setStaticImageEditing(true);
    refresh();
    emit editingStateChanged();
}

void StaticImageEditorPanel::cancelEditing()
{
    if (!m_session)
        return;
    m_session->source->setStaticImageEditing(false);
    m_session->editing = false;
    m_session->undo.clear();
    m_session->redo.clear();
    m_session->document = StaticImageDocument::fromObject(*m_session->source);
    m_part = -1;
    showOverview();
    refresh();
    emit editingStateChanged();
}

bool StaticImageEditorPanel::applyEditing()
{
    if (!editing())
        return false;
    auto source = m_session->source;
    const int index = ProjectManager::instance()->getObjects().indexOf(source);
    QString error;
    m_applying = true;
    const bool applied =
        ProjectManager::instance()->applyStaticImageDocument(index, m_session->document, &error);
    m_applying = false;
    if (!applied)
    {
        showError(error);
        return false;
    }
    source->setStaticImageEditing(false);
    m_sessions.remove(source.data());
    m_session.clear();
    setSelectedObjects({index});
    emit editingStateChanged();
    emit objectApplied(index);
    return true;
}

void StaticImageEditorPanel::saveHistory()
{
    if (!editing())
        return;
    m_session->undo.append(m_session->document);
    m_session->redo.clear();
    if (m_session->undo.size() > 80)
        m_session->undo.removeFirst();
}

void StaticImageEditorPanel::undoDraft(bool redo)
{
    if (!editing())
        return;
    auto &from = redo ? m_session->redo : m_session->undo;
    auto &to = redo ? m_session->undo : m_session->redo;
    if (from.isEmpty())
        return;
    const int partId = m_part >= 0 && m_part < m_session->document.regions.size()
                           ? m_session->document.regions[m_part].id
                           : -1;
    to.append(m_session->document);
    m_session->document = from.takeLast();
    if (partId >= 0)
    {
        m_part = -1;
        for (int i = 0; i < m_session->document.regions.size(); ++i)
            if (m_session->document.regions[i].id == partId)
                m_part = i;
        if (m_part < 0)
            showOverview();
    }
    refresh();
}

void StaticImageEditorPanel::showError(const QString &message)
{
    m_hint->setProperty("severity", "warning");
    m_hint->setText(message);
    m_hint->style()->unpolish(m_hint);
    m_hint->style()->polish(m_hint);
}

void StaticImageEditorPanel::refresh(bool rebuildList)
{
    m_updating = true;
    const auto *document = draft();
    m_canvas->setDocument(document);
    m_canvas->setEditable(editing());
    m_begin->setVisible(!editing());
    m_begin->setEnabled(m_session && !m_session->source->isSelectionLocked());
    m_apply->setVisible(editing() && !m_inDetail);
    m_cancel->setVisible(editing() && !m_inDetail);
    for (auto *button : {m_mergeTool, m_empty})
        button->setEnabled(editing());
    m_undo->setEnabled(editing() && !m_session->undo.isEmpty());
    m_redo->setEnabled(editing() && !m_session->redo.isEmpty());
    m_name->setText(m_session
                        ? (m_session->source->customName().isEmpty() ? m_session->source->getDisplayName()
                                                                     : m_session->source->customName())
                        : tr("Статическое изображение"));
    if (!document)
    {
        m_stats->hide();
        m_statsEmpty->show();
        m_hint->clear();
        m_partCanvas->setDocument(nullptr);
        m_masks->clear();
        m_updating = false;
        updateMaskActions();
        return;
    }
    m_stats->show();
    m_statsEmpty->hide();
    int excluded = 0;
    for (const auto &region : document->regions)
        excluded += region.excluded;
    const auto saved = document->originalCells - document->cells();
    const double percent = document->originalCells ? 100.0 * saved / document->originalCells : 0;
    const QLocale locale;
    auto number = [&](qint64 value) { return locale.toString(value); };
    m_statValues["size"]->setText(tr("%1 × %2").arg(document->size.width()).arg(document->size.height()));
    m_statValues["regions"]->setText(number(document->regions.size()));
    m_statValues["excluded"]->setText(number(excluded));
    m_statValues["before"]->setText(number(document->originalCells));
    m_statValues["after"]->setText(number(document->cells()));
    m_statValues["saved"]->setText(number(saved));
    m_statValues["percent"]->setText(locale.toString(percent, 'f', 1) + "%");
    for (const auto &key : {QStringLiteral("saved"), QStringLiteral("percent")})
    {
        auto *value = m_statValues[key];
        value->setProperty("saving", saved > 0);
        value->setProperty("growth", saved < 0);
        value->style()->unpolish(value);
        value->style()->polish(value);
    }
    m_statValues["groups"]->setText(number(document->exportedRegions().size()));
    m_statValues["init"]->setText(number(document->initCount()));
    m_statValues["bits"]->setText(number(document->cells() * 3));
    m_statValues["uart"]->setText(number(document->cells()));
    m_apply->setEnabled(editing() && document->hardwareErrors().isEmpty());
    updateHint();
    if (m_part >= 0 && m_part < document->regions.size())
    {
        const auto &region = document->regions[m_part];
        m_partName->setText(tr("%1 · %2 init").arg(region.name).arg(region.masks.size()));
        m_partCanvas->setDocument(document, m_part);
        m_partCanvas->setEditable(editing());
        if (rebuildList)
        {
            const auto selection = selectedMasks();
            const int current = m_masks->currentRow();
            QSignalBlocker blocker(m_masks);
            m_masks->clear();
            for (int i = 0; i < region.masks.size(); ++i)
            {
                const auto &mask = region.masks[i];
                auto *item = new QListWidgetItem(tr("%1 · %2\n%3 × %4 · %5 ячеек%6")
                                                     .arg(i + 1)
                                                     .arg(mask.color.name())
                                                     .arg(mask.rect.width())
                                                     .arg(mask.rect.height())
                                                     .arg(mask.data.size())
                                                     .arg(mask.enabled ? QString() : tr(" · выключен")),
                                                 m_masks);
                item->setData(Qt::UserRole, i);
                item->setData(Qt::UserRole + 1, mask.color);
                item->setToolTip(item->text());
            }
            if (current >= 0 && current < m_masks->count())
                m_masks->setCurrentRow(current, QItemSelectionModel::NoUpdate);
            for (int index : selection)
                if (auto *item = m_masks->item(index))
                    item->setSelected(true);
            m_partCanvas->setSelectedMasks(selectedMasks());
        }
        m_masks->setDragEnabled(editing());
        m_color->setEnabled(editing() && !m_masks->selectedItems().isEmpty());
        m_merge->setEnabled(editing() && m_masks->selectedItems().size() > 1);
    }
    m_updating = false;
    updateMaskActions();
}

void StaticImageEditorPanel::animatePage(bool detail)
{
    m_transition->stop();
    m_inDetail = detail;
    m_apply->setVisible(editing() && !detail);
    m_cancel->setVisible(editing() && !detail);
    m_begin->setVisible(!editing() && !detail);
    updateHint();
    if (detail)
    {
        m_pages->setCurrentWidget(m_detail);
        m_detail->move(m_pages->width(), 0);
        m_transition->setStartValue(QPoint(m_pages->width(), 0));
        m_transition->setEndValue(QPoint());
    }
    else
    {
        m_pages->setCurrentWidget(m_overview);
        m_detail->show();
        m_detail->raise();
        m_detail->setGeometry(m_pages->rect());
        m_transition->setStartValue(QPoint());
        m_transition->setEndValue(QPoint(m_pages->width(), 0));
    }
    m_transition->start();
}

void StaticImageEditorPanel::openRegion(int region)
{
    if (!m_session || region < 0 || region >= m_session->document.regions.size())
        return;
    if (!editing())
        beginEditing();
    if (!editing())
        return;
    m_part = region;
    refresh();
    animatePage(true);
}

void StaticImageEditorPanel::showOverview()
{
    if (m_inDetail)
        animatePage(false);
    m_part = -1;
}

QSet<int> StaticImageEditorPanel::selectedMasks() const
{
    QSet<int> indexes;
    for (auto *item : m_masks->selectedItems())
        indexes.insert(m_masks->row(item));
    return indexes;
}

void StaticImageEditorPanel::selectMasks(const QList<int> &indexes, int target)
{
    QSignalBlocker blocker(m_masks);
    m_masks->clearSelection();
    if (target >= 0 && target < m_masks->count())
        m_masks->setCurrentRow(target, QItemSelectionModel::NoUpdate);
    for (int index : indexes)
        if (auto *item = m_masks->item(index))
            item->setSelected(true);
    m_partCanvas->setSelectedMasks(QSet<int>(indexes.begin(), indexes.end()));
    updateMaskActions();
}

void StaticImageEditorPanel::updateMaskActions()
{
    const auto selection = selectedMasks();
    m_merge->setEnabled(editing() && selection.size() > 1);
    m_color->setEnabled(editing() && !selection.isEmpty());
    const bool canMove = editing() && !selection.isEmpty();
    const int first = canMove ? *std::min_element(selection.begin(), selection.end()) : -1;
    const int last = canMove ? *std::max_element(selection.begin(), selection.end()) : -1;
    m_top->setEnabled(canMove && first > 0);
    m_up->setEnabled(canMove && first > 0);
    m_down->setEnabled(canMove && last < m_masks->count() - 1);
    m_bottom->setEnabled(canMove && last < m_masks->count() - 1);
    int target = m_masks->currentRow();
    if (!selection.contains(target) && !selection.isEmpty())
        target = last;
    QString tip = tr("Объединить выбранные маски");
    if (m_session && m_part >= 0 && m_part < m_session->document.regions.size() && target >= 0 &&
        target < m_session->document.regions[m_part].masks.size() && selection.size() > 1)
        tip = tr("Объединить в цвет %1").arg(m_session->document.regions[m_part].masks[target].color.name());
    m_merge->setToolTip(tip);
    m_merge->setAccessibleName(tip);
}

void StaticImageEditorPanel::moveMasks(int direction, bool edge)
{
    if (!editing() || !m_inDetail || selectedMasks().isEmpty() || m_reorderPending)
        return;
    auto rows = selectedMasks().values();
    std::sort(rows.begin(), rows.end());
    if (edge)
        static_cast<StaticMaskList *>(m_masks)->moveSelectionTo(direction < 0 ? 0 : m_masks->count());
    else if (direction < 0 && rows.first() > 0)
        for (int row : rows)
            m_masks->model()->moveRow({}, row, {}, row - 1);
    else if (direction > 0 && rows.last() + 1 < m_masks->count())
        for (auto it = rows.crbegin(); it != rows.crend(); ++it)
            m_masks->model()->moveRow({}, *it, {}, *it + 2);
    if (m_masks->currentItem())
        m_masks->scrollToItem(m_masks->currentItem());
}

void StaticImageEditorPanel::updateHint()
{
    const auto errors = draft() ? draft()->hardwareErrors() : QStringList();
    m_hint->setProperty("severity", errors.isEmpty() ? "normal" : "error");
    m_hint->setText(!errors.isEmpty()
                        ? errors.join('\n')
                        : (m_inDetail ? tr("Перетаскивайте маски между строками. Alt+↑/↓ — порядок; "
                                           "Alt+Shift+↑/↓ — в начало/конец.")
                           : editing() ? tr("Линейка → нужная область — разрез. H / V + щелчок — разрез на "
                                            "месте. Щелчок — исключить; двойной — маски.")
                                       : tr("Начните редактирование, чтобы разбить изображение на части и "
                                            "исключить пустые области.")));
    m_hint->style()->unpolish(m_hint);
    m_hint->style()->polish(m_hint);
}

void StaticImageEditorPanel::mergeMasks(const QList<int> &indexes, int target)
{
    if (!editing() || m_part < 0 || indexes.size() < 2)
        return;
    if (!indexes.contains(target))
        target = *std::max_element(indexes.begin(), indexes.end());
    if (target < 0 || target >= m_session->document.regions[m_part].masks.size())
        return;
    auto next = m_session->document;
    const auto color = next.regions[m_part].masks[target].color;
    if (!next.mergeMasks(m_part, indexes, color))
        return;
    saveHistory();
    m_session->document = next;
    refresh();
}

void StaticImageEditorPanel::changeColor()
{
    if (!editing() || m_part < 0)
        return;
    const auto indexes = selectedMasks();
    if (indexes.isEmpty())
        return;
    const QColor color = QColorDialog::getColor(
        m_session->document.regions[m_part].masks[*indexes.begin()].color, this, tr("Цвет маски"));
    if (!color.isValid())
        return;
    saveHistory();
    for (int index : indexes)
        m_session->document.regions[m_part].masks[index].color = color;
    refresh();
}

void StaticImageEditorPanel::reorderMasks()
{
    if (m_updating || !editing() || m_part < 0 || m_reorderPending)
        return;
    m_reorderPending = true;
    const auto session = m_session;
    const int part = m_part;
    // Один перенос нескольких выбранных строк может испустить несколько rowsMoved.
    // Считываем окончательную перестановку один раз, сохраняя исходные идентификаторы строк.
    QTimer::singleShot(0, this,
                       [this, session, part]
                       {
                           m_reorderPending = false;
                           if (!editing() || m_session != session || m_part != part)
                               return;
                           const auto masks = m_session->document.regions[m_part].masks;
                           QList<StaticImageMask> reordered;
                           QSet<int> seen;
                           for (int i = 0; i < m_masks->count(); ++i)
                           {
                               const int index = m_masks->item(i)->data(Qt::UserRole).toInt();
                               if (index < 0 || index >= masks.size() || seen.contains(index))
                                   return;
                               seen.insert(index);
                               reordered.append(masks[index]);
                           }
                           if (reordered.size() != masks.size())
                               return;
                           bool changed = false;
                           for (int i = 0; i < m_masks->count(); ++i)
                               changed |= m_masks->item(i)->data(Qt::UserRole).toInt() != i;
                           if (!changed)
                               return;
                           saveHistory();
                           m_session->document.regions[m_part].masks = reordered;
                           refresh();
                       });
}
