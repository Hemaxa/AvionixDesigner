// StaticImageEditorPanel — черновики, навигация, маски и применение к сцене.
#include "AppearanceManager.h"
#include "StaticImageEditorPanel.h"
#include "ExpressionSpinBox.h"
#include "ElidedLabel.h"
#include "ImageObject.h"
#include "OptimizedStaticImageObject.h"
#include "ProjectManager.h"
#include "StaticGroupObject.h"
#include "StaticImageCanvas.h"
#include <QAbstractItemModel>
#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QDataStream>
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
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
// The stamp reads metadata and shared-storage identities, never the mask pixels during a move.
QByteArray sourceKey(const BaseObject &object)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    if (const auto *image = dynamic_cast<const ImageObject *>(&object))
        stream << image->rasterRevision();
    else if (const auto *optimized = dynamic_cast<const OptimizedStaticImageObject *>(&object))
    {
        const auto &document = optimized->document;
        stream << document.size << document.originalCells << document.nextId
               << quint64(reinterpret_cast<quintptr>(document.regions.constData()));
    }
    else if (const auto *group = dynamic_cast<const StaticGroupObject *>(&object))
    {
        const auto origin = StaticImageDocument::objectOrigin(object);
        stream << group->states.size();
        for (int i = 0; i < group->states.size(); ++i)
        {
            const auto &state = group->states[i];
            stream << QPointF(state.x, state.y) - origin << state.w << state.h << state.addr << state.color
                   << state.enabled << group->maskImages.value(i).cacheKey();
        }
    }
    return result;
}

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
        QPointer<StaticMaskList> alive(this);
        QPointer<QDrag> drag = new QDrag(this);
        drag->setMimeData(model()->mimeData(selectedIndexes()));
        if (currentItem())
        {
            const auto preview = visualItemRect(currentItem()).intersected(viewport()->rect());
            if (!preview.isEmpty())
            {
                drag->setPixmap(viewport()->grab(preview));
                drag->setHotSpot(QPoint(24, preview.height() / 2));
            }
        }
        drag->exec(Qt::MoveAction);
        if (drag)
            drag->deleteLater();
        // Closing the editor during the native drag may destroy the view and its owned drag.
        if (!alive)
            return;
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
        painter.setPen(AppearanceManager::instance()->pen("pen.emphasis", AppearanceManager::instance()->color("accent")));
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
        painter->setPen(AppearanceManager::instance()->pen("pen.thin", AppearanceManager::instance()->color("muted")));
        painter->setBrush(index.data(Qt::UserRole + 1).value<QColor>());
        const double radius = AppearanceManager::instance()->number("static.swatchRadius");
        painter->drawRoundedRect(swatch, radius, radius);
        const auto text = option.rect.adjusted(46, 9, -10, -9);
        painter->setPen(AppearanceManager::instance()->color("text"));
        painter->drawText(text, Qt::AlignLeft | Qt::AlignTop,
                          option.fontMetrics.elidedText(lines.value(0), Qt::ElideRight, text.width()));
        painter->setPen(AppearanceManager::instance()->color("muted"));
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
    auto *headerWidget = new QWidget(this);
    headerWidget->setObjectName("StaticEditorHeader");
    auto *header = new QHBoxLayout(headerWidget);
    header->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(headerWidget);
    auto *back = new QToolButton(headerWidget);
    back->setIcon(QIcon(":/icons/icons/static-editor/back.svg"));
    back->setObjectName("StaticPartBack");
    auto backPolicy = back->sizePolicy();
    backPolicy.setRetainSizeWhenHidden(true);
    back->setSizePolicy(backPolicy);
    back->setToolTip(tr("К разбиению"));
    back->setAccessibleName(back->toolTip());
    back->hide();
    header->addWidget(back);
    m_name = new ElidedLabel(tr("Статическое изображение"), headerWidget);
    m_name->setObjectName("StaticEditorTitle");
    m_name->setTextFormat(Qt::PlainText);
    header->addWidget(m_name, 1);
    m_partName = new QLabel(this);
    m_partName->setObjectName("StaticSectionTitle");
    m_partName->setTextFormat(Qt::PlainText);
    m_partName->hide();
    header->addWidget(m_partName);
    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(8);
    layout->addLayout(toolbar);
    auto group = [&](const QString &title)
    {
        auto *frame = new QFrame(this);
        frame->setObjectName("StaticToolGroup");
        frame->setAccessibleName(title);
        auto *bar = new QHBoxLayout(frame);
        bar->setContentsMargins(8, 8, 8, 8);
        bar->setSpacing(6);
        toolbar->addWidget(frame);
        return bar;
    };
    auto tool =
        [&](QHBoxLayout *bar, QWidget *parent, const char *name, const QString &icon, const QString &tip)
    {
        auto *button = new QToolButton(parent);
        button->setObjectName(name);
        button->setIcon(QIcon(icon));
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        bar->addWidget(button);
        return button;
    };
    auto *historyBar = group(tr("История"));
    m_undo = tool(historyBar, this, "StaticDraftUndo", ":/icons/icons/selection/undo.svg",
                  tr("Отменить изменение в окне"));
    m_redo = tool(historyBar, this, "StaticDraftRedo", ":/icons/icons/selection/redo.svg",
                  tr("Повторить изменение в окне"));
    auto *editBar = group(tr("Инструменты"));
    m_selectTool =
        tool(editBar, this, "StaticSelectTool", ":/icons/icons/static-editor/select.svg", tr("Выбор"));
    m_mergeTool = tool(editBar, this, "StaticShapeBuilder", ":/icons/icons/static-editor/shape-builder.svg",
                       tr("Объединить области жестом"));
    m_partTool = tool(editBar, this, "StaticMaskShapeBuilder",
                      ":/icons/icons/static-editor/shape-builder.svg", tr("Объединить маски жестом"));
    m_extendTool = tool(editBar, this, "StaticExtendCut", ":/icons/icons/static-editor/extend-cut.svg",
                        tr("Продлить разрез: нажмите на существующую линию"));
    m_empty = tool(editBar, this, "StaticExcludeEmpty", ":/icons/icons/static-editor/exclude-empty.svg",
                   tr("Исключить области с полностью нулевыми масками"));
    m_eraser = tool(editBar, this, "StaticEraser", ":/icons/icons/static-editor/eraser.svg",
                    tr("Ластик: стереть пиксели выбранных масок"));
    m_brushControls = new QWidget(this);
    m_brushControls->setObjectName("StaticEraserControls");
    auto *brushLayout = new QHBoxLayout(m_brushControls);
    brushLayout->setContentsMargins(0, 0, 0, 0);
    brushLayout->setSpacing(6);
    m_brushSlider = new QSlider(Qt::Horizontal, m_brushControls);
    m_brushSlider->setObjectName("StaticEraserSlider");
    m_brushSlider->setRange(1, 1);
    m_brushSlider->setAccessibleName(tr("Размер ластика"));
    m_brushValue = new QLabel(m_brushControls);
    m_brushValue->setObjectName("StaticEraserValue");
    brushLayout->addWidget(m_brushSlider, 1);
    brushLayout->addWidget(m_brushValue);
    editBar->addWidget(m_brushControls);
    for (auto *button : {m_selectTool, m_mergeTool, m_partTool, m_extendTool, m_eraser})
        button->setCheckable(true);
    m_selectTool->setChecked(true);
    m_partTool->hide();
    m_eraser->hide();
    m_brushControls->hide();
    auto *viewBar = group(tr("Просмотр"));
    m_hand = tool(viewBar, this, "StaticPan", ":/icons/icons/static-editor/hand.svg",
                  tr("Панорама · также Пробел + перетаскивание, правая или средняя кнопка"));
    m_hand->setCheckable(true);
    auto *fit =
        tool(viewBar, this, "StaticFit", ":/icons/icons/static-editor/fit.svg", tr("Вписать изображение"));
    m_before = tool(viewBar, this, "StaticPreviewBefore", ":/icons/icons/static-editor/preview-before.svg",
                    tr("До изменений"));
    m_after = tool(viewBar, this, "StaticPreviewAfter", ":/icons/icons/static-editor/preview-after.svg",
                   tr("После изменений"));
    m_before->setCheckable(true);
    m_after->setCheckable(true);
    m_after->setChecked(true);
    toolbar->addStretch();
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
    auto *sideScroll = new QScrollArea(m_overview);
    sideScroll->setObjectName("StaticStatsScroll");
    sideScroll->setWidgetResizable(true);
    sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideScroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    overview->addWidget(sideScroll);
    auto *side = new QWidget;
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(8);
    sideScroll->setWidget(side);
    auto *cutCard = new QFrame;
    cutCard->setObjectName("StaticCutCard");
    sideLayout->addWidget(cutCard);
    auto *cutLayout = new QVBoxLayout(cutCard);
    cutLayout->setContentsMargins(10, 10, 10, 10);
    cutLayout->setSpacing(6);
    cutCard->setAccessibleName(tr("Разрез"));
    m_cutRegion = new QComboBox;
    m_cutRegion->setObjectName("StaticCutRegion");
    m_cutRegion->setToolTip(tr("Область разреза"));
    cutLayout->addWidget(m_cutRegion);
    m_cutAxis = new QComboBox;
    m_cutAxis->setObjectName("StaticCutAxis");
    m_cutAxis->addItem(tr("Вертикальный"), int(Qt::Vertical));
    m_cutAxis->addItem(tr("Горизонтальный"), int(Qt::Horizontal));
    cutLayout->addWidget(m_cutAxis);
    auto *coordinate = new QHBoxLayout;
    coordinate->setSpacing(6);
    m_coordinateLabel = new QLabel("X");
    coordinate->addWidget(m_coordinateLabel);
    m_cutPosition = new ExpressionSpinBox;
    m_cutPosition->setObjectName("StaticCutCoordinate");
    m_cutPosition->setSuffix(" px");
    coordinate->addWidget(m_cutPosition, 1);
    m_split = tool(coordinate, cutCard, "StaticSplitExact", ":/icons/icons/static-editor/split.svg",
                   tr("Разрезать по заданной координате"));
    cutLayout->addLayout(coordinate);
    sideLayout->addSpacing(8);
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
        value->setProperty("statistic", key);
        value->setAccessibleName(title);
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
    sideLayout->addStretch();
    auto *detailLayout = new QVBoxLayout(m_detail);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    auto *body = new QSplitter(m_detail);
    body->setChildrenCollapsible(false);
    body->setObjectName("StaticMaskSplitter");
    detailLayout->addWidget(body, 1);
    m_partCanvas = new StaticImageCanvas(body);
    m_partCanvas->setObjectName("StaticMaskCanvas");
    auto *maskPanel = new QWidget(body);
    maskPanel->setObjectName("StaticMaskSidePanel");
    auto *maskSide = new QVBoxLayout(maskPanel);
    maskSide->setContentsMargins(0, 0, 0, 0);
    maskSide->setSpacing(8);
    auto *maskHeader = new QHBoxLayout;
    maskSide->addLayout(maskHeader);
    auto *maskTitle = new QLabel(tr("Маски части"));
    maskTitle->setObjectName("StaticSectionTitle");
    maskHeader->addWidget(maskTitle, 1);
    auto *inspect = new QHBoxLayout;
    inspect->setSpacing(6);
    maskSide->addLayout(inspect);
    m_color = tool(inspect, maskPanel, "StaticMaskColor", ":/icons/icons/static-editor/color.svg",
                   tr("Изменить цвет выбранных масок"));
    m_merge = tool(inspect, maskPanel, "StaticMergeMasks", ":/icons/icons/static-editor/merge-masks.svg",
                   tr("Объединить выбранные маски"));
    m_isolate = tool(inspect, maskPanel, "StaticIsolateMask", ":/icons/icons/static-editor/isolate.svg",
                     tr("Показать только выбранные маски"));
    m_isolate->setCheckable(true);
    inspect->addStretch();
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
    m_maskInfo = new QLabel;
    m_maskInfo->setObjectName("StaticMaskInfo");
    m_maskInfo->setWordWrap(true);
    m_maskInfo->hide();
    maskSide->addWidget(m_maskInfo);
    body->setStretchFactor(0, 1);
    body->setStretchFactor(1, 0);
    body->setSizes({650, 300});

    m_hint = new QLabel(this);
    m_hint->setObjectName("StaticEditorHint");
    m_hint->setWordWrap(true);
    m_hint->hide();
    auto *footer = new QHBoxLayout;
    layout->addLayout(footer);
    footer->addWidget(m_hint, 1);
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
    connect(back, &QToolButton::clicked, this, &StaticImageEditorPanel::showOverview);
    connect(fit, &QToolButton::clicked, this, [this] { (m_inDetail ? m_partCanvas : m_canvas)->fit(); });
    for (const auto &entry : {qMakePair(m_selectTool, 0), qMakePair(m_mergeTool, 1), qMakePair(m_partTool, 1),
                              qMakePair(m_extendTool, 2), qMakePair(m_eraser, 3), qMakePair(m_hand, 4)})
        connect(entry.first, &QToolButton::clicked, this,
                [this, mode = entry.second](bool checked) { chooseTool(checked ? mode : 0); });
    connect(m_before, &QToolButton::clicked, this, [this] { showBefore(true); });
    connect(m_after, &QToolButton::clicked, this, [this] { showBefore(false); });
    connect(m_isolate, &QToolButton::toggled, m_partCanvas, &StaticImageCanvas::setIsolated);
    connect(m_brushSlider, &QSlider::valueChanged, m_partCanvas, &StaticImageCanvas::setBrushSize);
    connect(m_brushSlider, &QSlider::valueChanged, this, &StaticImageEditorPanel::updateBrushControls);
    connect(m_partCanvas, &StaticImageCanvas::brushSizeChanged, m_brushSlider, &QSlider::setValue);
    connect(m_partCanvas, &StaticImageCanvas::selectionInfoChanged, this,
            [this](const QString &info)
            {
                m_maskInfo->setText(info);
                m_maskInfo->setVisible(!info.isEmpty());
            });
    connect(m_partCanvas, &StaticImageCanvas::eraseFinished, this,
            [this](const StaticImageDocument &document)
            {
                if (!editing() || m_showBefore)
                    return;
                saveHistory();
                m_session->document = document;
                refresh();
            });
    connect(m_canvas, &StaticImageCanvas::cutsExtensionRequested, this,
            [this](QList<int> regions, Qt::Orientation orientation, int coordinate)
            {
                if (!editing() || m_showBefore)
                    return;
                auto next = m_session->document;
                std::sort(regions.begin(), regions.end(), std::greater<int>());
                if (regions.isEmpty())
                    return;
                for (int region : regions)
                    if (!next.split(region, orientation, coordinate))
                    {
                        showError(tr("Не удалось продлить разрез: достигнут предел количества областей."));
                        return;
                    }
                saveHistory();
                m_session->document = next;
                refresh();
            });
    connect(m_cutRegion, &QComboBox::currentIndexChanged, this, [this] { updateCutControls(true); });
    connect(m_cutAxis, &QComboBox::currentIndexChanged, this, [this] { updateCutControls(true); });
    connect(m_cutPosition, &QSpinBox::valueChanged, this, [this] { updateCutControls(true); });
    connect(
        m_split, &QToolButton::clicked, this,
        [this]
        {
            if (!editing() || m_showBefore)
                return;
            const int id = m_cutRegion->currentData().toInt();
            auto next = m_session->document;
            for (int i = 0; i < next.regions.size(); ++i)
                if (next.regions[i].id == id &&
                    next.split(i, Qt::Orientation(m_cutAxis->currentData().toInt()), m_cutPosition->value()))
                {
                    saveHistory();
                    m_session->document = next;
                    refresh();
                    break;
                }
        });
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
    connect(m_color, &QToolButton::clicked, this, &StaticImageEditorPanel::changeColor);
    connect(m_masks, &QListWidget::itemDoubleClicked, this, [this] { changeColor(); });
    connect(m_merge, &QToolButton::clicked, this,
            [this] { mergeMasks(selectedMasks().values(), m_masks->currentRow()); });
    connect(m_top, &QToolButton::clicked, this, [this] { moveMasks(-1, true); });
    connect(m_up, &QToolButton::clicked, this, [this] { moveMasks(-1); });
    connect(m_down, &QToolButton::clicked, this, [this] { moveMasks(1); });
    connect(m_bottom, &QToolButton::clicked, this, [this] { moveMasks(1, true); });
    connect(m_pages, &QStackedWidget::currentChanged, this,
            [this, back]
            {
                const bool detail = m_pages->currentWidget() == m_detail;
                m_mergeTool->setVisible(!detail);
                m_empty->setVisible(!detail);
                m_extendTool->setVisible(!detail);
                m_partTool->setVisible(detail);
                m_eraser->setVisible(detail);
                m_brushControls->setVisible(detail);
                m_partName->setVisible(detail);
                back->setVisible(detail);
                if ((!detail && m_toolMode == 3) || (detail && m_toolMode == 2))
                    chooseTool(0);
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
        {
            // The window tree is being destroyed; do not reenter its project/UI connections.
            QSignalBlocker blocker(session->source.data());
            session->source->setStaticImageEditing(false);
        }
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
    bool removedCurrent = false;
    for (auto it = m_sessions.begin(); it != m_sessions.end();)
    {
        const auto session = it.value();
        const bool removed = !objects.contains(session->source);
        if (removed || (!session->editing && session != m_session))
        {
            if (removed && session->source->isStaticImageEditing())
            {
                QSignalBlocker blocker(session->source.data());
                session->source->setStaticImageEditing(false);
            }
            if (session == m_session)
            {
                // Detach borrowed document pointers before releasing their session.
                m_canvas->setDocument(nullptr);
                m_partCanvas->setDocument(nullptr);
                m_session.clear();
                removedCurrent = true;
            }
            it = m_sessions.erase(it);
        }
        else
            ++it;
    }
    if (removedCurrent)
    {
        m_transition->stop();
        m_inDetail = false;
        m_part = -1;
        m_pages->setCurrentWidget(m_overview);
        m_detail->hide();
        refresh();
    }
}

void StaticImageEditorPanel::trimHistory()
{
    if (!m_session)
        return;
    // Bound both the action count and the retained mask bytes; large eraser strokes detach data.
    constexpr qint64 byteLimit = 256LL * 1024 * 1024;
    auto bytes = [](const StaticImageDocument &document)
    {
        qint64 result = 0;
        for (const auto &region : document.regions)
            for (const auto &mask : region.masks)
                result += mask.data.size();
        return result;
    };
    auto &undo = m_session->undo;
    auto &redo = m_session->redo;
    while (undo.size() > 80)
        undo.removeFirst();
    while (redo.size() > 80)
        redo.removeFirst();
    qint64 retained = 0;
    for (const auto &document : undo)
        retained += bytes(document);
    for (const auto &document : redo)
        retained += bytes(document);
    while (retained > byteLimit && (undo.size() > 1 || redo.size() > 1))
    {
        auto &history = undo.size() > 1 ? undo : redo;
        retained -= bytes(history.first());
        history.removeFirst();
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
        if (m_session && !m_session->editing)
            m_sessions.remove(m_session->source.data());
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
    const auto key = sourceKey(*source);
    if (!m_session->editing && m_session->sourceKey == key)
    {
        m_name->setText(source->customName().isEmpty() ? source->getDisplayName() : source->customName());
        m_begin->setEnabled(!source->isSelectionLocked());
        return;
    }
    if (!m_session->editing)
    {
        QString error;
        m_session->document = StaticImageDocument::fromObject(*source, &error);
        m_session->beforeImage = m_session->document.render();
        m_session->beforeStatistics = statisticsFor(m_session->document);
        m_session->sourceKey = key;
        if (!error.isEmpty())
        {
            m_sessions.remove(source.data());
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
    m_session->beforeStatistics = statisticsFor(m_session->document);
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
    m_session->sourceKey = sourceKey(*m_session->source);
    m_session->beforeImage = m_session->document.render();
    m_session->beforeStatistics = statisticsFor(m_session->document);
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
    trimHistory();
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
    trimHistory();
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
    m_hint->show();
    m_hint->style()->unpolish(m_hint);
    m_hint->style()->polish(m_hint);
}

void StaticImageEditorPanel::refresh(bool rebuildList)
{
    m_updating = true;
    const auto *document = draft();
    m_canvas->setDocument(document);
    m_canvas->setEditable(editing());
    m_canvas->setBeforeImage(m_session ? m_session->beforeImage : QImage());
    m_canvas->setShowBefore(m_showBefore);
    m_before->setEnabled(bool(m_session));
    m_after->setEnabled(bool(m_session));
    for (auto *button : {m_mergeTool, m_partTool, m_empty, m_extendTool})
        button->setEnabled(editing() && !m_showBefore);
    const int selectedId = m_cutRegion->currentData().toInt();
    {
        QSignalBlocker blocker(m_cutRegion);
        m_cutRegion->clear();
        if (document)
            for (const auto &region : document->regions)
                m_cutRegion->addItem(
                    tr("%1 · %2 × %3").arg(region.name).arg(region.rect.width()).arg(region.rect.height()),
                    region.id);
        m_cutRegion->setCurrentIndex(qMax(0, m_cutRegion->findData(selectedId)));
    }
    updateCutControls();
    m_begin->setVisible(!editing());
    m_begin->setEnabled(m_session && !m_session->source->isSelectionLocked());
    m_apply->setVisible(editing() && !m_inDetail);
    m_cancel->setVisible(editing() && !m_inDetail);
    m_undo->setEnabled(editing() && !m_session->undo.isEmpty());
    m_redo->setEnabled(editing() && !m_session->redo.isEmpty());
    m_name->setText(m_session
                        ? (m_session->source->customName().isEmpty() ? m_session->source->getDisplayName()
                                                                     : m_session->source->customName())
                        : tr("Статическое изображение"));
    m_name->setToolTip(m_name->text());
    if (!document)
    {
        m_stats->hide();
        m_statsEmpty->show();
        m_hint->clear();
        m_hint->hide();
        m_partCanvas->setDocument(nullptr);
        m_masks->clear();
        m_updating = false;
        updateMaskActions();
        return;
    }
    m_stats->show();
    m_statsEmpty->hide();
    updateStatistics();
    m_apply->setEnabled(editing() && !m_showBefore && document->hardwareErrors().isEmpty());
    updateHint();
    if (m_part >= 0 && m_part < document->regions.size())
    {
        const auto &region = document->regions[m_part];
        m_partName->setText(tr("%1 · %2 init").arg(region.name).arg(region.masks.size()));
        m_partCanvas->setDocument(document, m_part);
        m_partCanvas->setEditable(editing());
        m_partCanvas->setBeforeImage(m_session->beforeImage);
        m_partCanvas->setShowBefore(m_showBefore);
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
        m_masks->setDragEnabled(editing() && !m_showBefore);
        m_color->setEnabled(editing() && !m_masks->selectedItems().isEmpty());
        m_merge->setEnabled(editing() && m_masks->selectedItems().size() > 1);
    }
    m_updating = false;
    updateMaskActions();
}

StaticImageEditorPanel::ImageStatistics
StaticImageEditorPanel::statisticsFor(const StaticImageDocument &document)
{
    ImageStatistics result;
    result.size = document.size;
    result.originalCells = document.originalCells;
    result.regions = document.regions.size();
    for (const auto &region : document.regions)
    {
        result.excluded += region.excluded;
        if (region.excluded || region.masks.isEmpty())
            continue;
        ++result.groups;
        result.init += region.masks.size();
        result.cells += region.cells();
    }
    return result;
}

void StaticImageEditorPanel::updateStatistics()
{
    if (!m_session)
        return;
    const auto statistics = m_showBefore ? m_session->beforeStatistics : statisticsFor(m_session->document);
    const auto saved = statistics.originalCells - statistics.cells;
    const double percent = statistics.originalCells ? 100.0 * saved / statistics.originalCells : 0;
    const QLocale locale;
    auto number = [&](qint64 value) { return locale.toString(value); };
    m_statValues["size"]->setText(tr("%1 × %2").arg(statistics.size.width()).arg(statistics.size.height()));
    m_statValues["regions"]->setText(number(statistics.regions));
    m_statValues["excluded"]->setText(number(statistics.excluded));
    m_statValues["before"]->setText(number(statistics.originalCells));
    m_statValues["after"]->setText(number(statistics.cells));
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
    m_statValues["groups"]->setText(number(statistics.groups));
    m_statValues["init"]->setText(number(statistics.init));
    m_statValues["bits"]->setText(number(statistics.cells * 3));
    m_statValues["uart"]->setText(number(statistics.cells));
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
    m_masks->clear();
    refresh();
    if (m_masks->count())
        m_masks->setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
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
    m_merge->setEnabled(editing() && !m_showBefore && selection.size() > 1);
    m_color->setEnabled(editing() && !m_showBefore && !selection.isEmpty());
    m_eraser->setEnabled(editing() && !m_showBefore && !selection.isEmpty());
    m_isolate->setEnabled(!m_showBefore && !selection.isEmpty());
    m_brushControls->setEnabled(editing() && !m_showBefore && !selection.isEmpty());
    updateBrushControls();
    const bool canMove = editing() && !m_showBefore && !selection.isEmpty();
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

void StaticImageEditorPanel::updateBrushControls()
{
    const int maximum = draft() ? draft()->maximumEraserSize() : 1;
    m_brushSlider->setRange(1, maximum);
    m_brushSlider->setPageStep(qMax(1, (maximum - 1) / 10));
    m_brushValue->setText(tr("%1 px").arg(m_brushSlider->value()));
    m_brushSlider->setToolTip(tr("Размер ластика: %1 px · диапазон 1–%2 px (25% короткой стороны) · [ / ] · Ctrl/⌘ + колесо")
                                 .arg(m_brushSlider->value()).arg(maximum));
}

void StaticImageEditorPanel::moveMasks(int direction, bool edge)
{
    if (!editing() || m_showBefore || !m_inDetail || selectedMasks().isEmpty() || m_reorderPending)
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
    m_hint->setProperty("severity", "error");
    m_hint->setText(errors.mid(0, 2).join('\n'));
    m_hint->setToolTip(errors.join('\n'));
    m_hint->setVisible(!errors.isEmpty());
    m_hint->style()->unpolish(m_hint);
    m_hint->style()->polish(m_hint);
}

void StaticImageEditorPanel::chooseTool(int mode)
{
    m_toolMode = mode;
    for (const auto &entry : {qMakePair(m_selectTool, 0), qMakePair(m_mergeTool, 1), qMakePair(m_partTool, 1),
                              qMakePair(m_extendTool, 2), qMakePair(m_eraser, 3), qMakePair(m_hand, 4)})
    {
        QSignalBlocker blocker(entry.first);
        entry.first->setChecked(mode == entry.second);
    }
    for (auto *canvas : {m_canvas, m_partCanvas})
    {
        canvas->setMergeTool(mode == 1);
        canvas->setExtendTool(mode == 2);
        canvas->setEraserTool(mode == 3);
        canvas->setHandTool(mode == 4);
    }
    updateMaskActions();
}

void StaticImageEditorPanel::showBefore(bool before)
{
    if (before)
        m_isolate->setChecked(false);
    m_showBefore = before;
    m_before->setChecked(before);
    m_after->setChecked(!before);
    if (before && m_toolMode != 4)
        chooseTool(0);
    refresh(false);
    if (m_part >= 0)
        m_partCanvas->setSelectedMasks(selectedMasks());
}

void StaticImageEditorPanel::updateCutControls(bool preview)
{
    const auto *document = draft();
    int region = -1;
    if (document)
        for (int i = 0; i < document->regions.size(); ++i)
            if (document->regions[i].id == m_cutRegion->currentData().toInt())
                region = i;
    const bool vertical = m_cutAxis->currentData().toInt() == int(Qt::Vertical);
    const auto bounds = region >= 0 ? document->regions[region].rect : QRect();
    const int start = vertical ? bounds.x() : bounds.y(),
              length = vertical ? bounds.width() : bounds.height();
    const bool available = editing() && !m_showBefore && region >= 0;
    m_cutRegion->setEnabled(available);
    m_cutAxis->setEnabled(available);
    m_cutPosition->setEnabled(available && length > 1);
    m_split->setEnabled(available && length > 1);
    m_coordinateLabel->setText(vertical ? "X" : "Y");
    {
        QSignalBlocker blocker(m_cutPosition);
        const int value = m_cutPosition->value();
        m_cutPosition->setRange(length > 1 ? start + 1 : 0, length > 1 ? start + length - 1 : 0);
        if (value <= start || value >= start + length)
            m_cutPosition->setValue(start + length / 2);
    }
    if (preview && available && length > 1)
        m_canvas->setExactCut(region, vertical ? Qt::Vertical : Qt::Horizontal, m_cutPosition->value());
    else
        m_canvas->setExactCut(-1, Qt::Vertical, 0);
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
