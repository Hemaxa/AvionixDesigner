// StaticImageEditorPanel — черновики, навигация, маски и применение к сцене.
#include "StaticImageEditorPanel.h"
#include "StaticImageCanvas.h"
#include "ProjectManager.h"
#include "fpga/compiler/FpgaResourceLimits.h"
#include <QAbstractItemModel>
#include <QKeyEvent>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
QIcon colorIcon(const QColor &color)
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setPen(QColor("#334656"));
    painter.setBrush(color);
    painter.drawRoundedRect(QRect(2, 2, 20, 20), 4, 4);
    return QIcon(pixmap);
}
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
    auto *tools = new QVBoxLayout;
    tools->setSpacing(6);
    overview->addLayout(tools);
    auto tool = [&](const char *name, const QString &icon, const QString &tip)
    {
        auto *button = new QToolButton(m_overview);
        button->setObjectName(name);
        button->setIcon(QIcon(icon));
        button->setIconSize(QSize(24, 24));
        button->setFixedSize(40, 40);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        tools->addWidget(button);
        return button;
    };
    m_mergeTool = tool("StaticShapeBuilder", ":/icons/icons/static-editor/shape-builder.svg",
                       tr("Объединение: проведите по областям или маскам"));
    m_mergeTool->setCheckable(true);
    m_empty = tool("StaticExcludeEmpty", ":/icons/icons/selection/delete.svg",
                   tr("Пометить все области с нулевыми масками"));
    m_undo = tool("StaticDraftUndo", ":/icons/icons/selection/undo.svg", tr("Отменить изменение в окне"));
    m_redo = tool("StaticDraftRedo", ":/icons/icons/selection/redo.svg", tr("Повторить изменение в окне"));
    auto *fit = tool("StaticFit", ":/icons/icons/selection/align-hcenter.svg", tr("Вписать изображение"));
    tools->addStretch();
    auto *side = new QVBoxLayout;
    overview->addLayout(side);
    auto *statsTitle = new QLabel(tr("Данные изображения"), m_overview);
    side->addWidget(statsTitle);
    m_stats = new QLabel(m_overview);
    m_stats->setObjectName("StaticEditorStats");
    m_stats->setTextFormat(Qt::PlainText);
    m_stats->setWordWrap(true);
    m_stats->setMinimumWidth(175);
    m_stats->setMaximumWidth(260);
    m_stats->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    side->addWidget(m_stats);
    side->addStretch();

    auto *detailLayout = new QVBoxLayout(m_detail);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    auto *navigation = new QHBoxLayout;
    detailLayout->addLayout(navigation);
    auto *back = new QPushButton(QIcon(":/icons/icons/selection/undo.svg"), tr("К разбиению"), m_detail);
    back->setObjectName("StaticPartBack");
    navigation->addWidget(back);
    m_partName = new QLabel(m_detail);
    m_partName->setTextFormat(Qt::PlainText);
    navigation->addWidget(m_partName, 1);
    auto *partTool = new QToolButton(m_detail);
    partTool->setObjectName("StaticMaskShapeBuilder");
    partTool->setIcon(QIcon(":/icons/icons/static-editor/shape-builder.svg"));
    partTool->setCheckable(true);
    partTool->setFixedSize(36, 36);
    partTool->setToolTip(tr("Объединение масок: отпустите мышь на цвете результата"));
    navigation->addWidget(partTool);
    auto *partFit = new QPushButton(tr("Вписать"), m_detail);
    navigation->addWidget(partFit);
    auto *body = new QHBoxLayout;
    detailLayout->addLayout(body, 1);
    m_partCanvas = new StaticImageCanvas(m_detail);
    m_partCanvas->setObjectName("StaticMaskCanvas");
    body->addWidget(m_partCanvas, 1);
    auto *maskSide = new QVBoxLayout;
    body->addLayout(maskSide);
    auto *maskTitle = new QLabel(tr("Маски · первый init сверху"), m_detail);
    maskSide->addWidget(maskTitle);
    m_masks = new QListWidget(m_detail);
    m_masks->setObjectName("StaticMaskList");
    m_masks->setMinimumWidth(230);
    m_masks->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_masks->setDragDropMode(QAbstractItemView::InternalMove);
    m_masks->setDefaultDropAction(Qt::MoveAction);
    m_masks->setIconSize(QSize(24, 24));
    maskSide->addWidget(m_masks, 1);
    auto *maskActions = new QHBoxLayout;
    detailLayout->addLayout(maskActions);
    auto *up = new QPushButton(tr("Выше"), m_detail), *down = new QPushButton(tr("Ниже"), m_detail);
    m_color = new QPushButton(tr("Цвет"), m_detail);
    m_color->setObjectName("StaticMaskColor");
    m_color->setToolTip(tr("Выбрать цвет маски"));
    m_merge = new QPushButton(tr("Объединить маски"), m_detail);
    m_merge->setObjectName("StaticMergeMasks");
    auto *done = new QPushButton(tr("Готово"), m_detail);
    maskActions->addStretch();
    for (auto *button : {up, down, m_color, m_merge, done})
        maskActions->addWidget(button);

    m_hint = new QLabel(this);
    m_hint->setObjectName("StaticEditorHint");
    m_hint->setWordWrap(true);
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
    connect(done, &QPushButton::clicked, this, &StaticImageEditorPanel::showOverview);
    connect(fit, &QToolButton::clicked, m_canvas, &StaticImageCanvas::fit);
    connect(partFit, &QPushButton::clicked, m_partCanvas, &StaticImageCanvas::fit);
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
    connect(m_canvas, &StaticImageCanvas::hintChanged, m_hint, &QLabel::setText);
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
    auto moveMask = [this](int delta)
    {
        if (!editing() || m_part < 0)
            return;
        int from = m_masks->currentRow(), to = from + delta;
        if (from < 0 || to < 0 || to >= m_session->document.regions[m_part].masks.size())
            return;
        saveHistory();
        m_session->document.regions[m_part].masks.move(from, to);
        refresh();
        m_masks->setCurrentRow(to);
    };
    connect(up, &QPushButton::clicked, this, [moveMask] { moveMask(-1); });
    connect(down, &QPushButton::clicked, this, [moveMask] { moveMask(1); });
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
    to.append(m_session->document);
    m_session->document = from.takeLast();
    if (m_part >= m_session->document.regions.size())
        showOverview();
    refresh();
}

void StaticImageEditorPanel::showError(const QString &message)
{
    m_hint->setText(message);
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
        m_stats->setText(tr("Выберите один статический объект."));
        m_hint->clear();
        m_partCanvas->setDocument(nullptr);
        m_masks->clear();
        m_updating = false;
        return;
    }
    int excluded = 0;
    for (const auto &region : document->regions)
        excluded += region.excluded;
    const auto saved = document->originalCells - document->cells();
    const double percent = document->originalCells ? 100.0 * saved / document->originalCells : 0;
    m_stats->setText(tr("%1 × %2 пикселей\n\nОбластей: %3\nИсключено: %4\nSTATICGROUP: %5\nInit: %6\n\nДо: "
                        "%7 ячеек\nПосле: %8 ячеек\nЭкономия: %9 (%10%)\n\nМаски: %11 бит\nUART масок: %12 "
                        "байт\nБанков: %5\n\nОбъём BRAM зависит от глубины банков в прошивке.")
                         .arg(document->size.width())
                         .arg(document->size.height())
                         .arg(document->regions.size())
                         .arg(excluded)
                         .arg(document->exportedRegions().size())
                         .arg(document->initCount())
                         .arg(document->originalCells)
                         .arg(document->cells())
                         .arg(saved)
                         .arg(percent, 0, 'f', 1)
                         .arg(document->cells() * 3)
                         .arg(document->cells()));
    const auto errors = document->hardwareErrors();
    m_apply->setEnabled(editing() && errors.isEmpty());
    m_hint->setText(!errors.isEmpty()
                        ? errors.join('\n')
                        : (editing()
                               ? tr("Черновик · линейки — разрезы, стык — линия, область — крестик, двойной "
                                    "щелчок — маски.")
                               : tr("Начните редактирование, чтобы зафиксировать размер и изменить маски.")));
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
                auto *item = new QListWidgetItem(colorIcon(mask.color),
                                                 tr("%1 · %2\n%3 × %4 · %5 ячеек%6")
                                                     .arg(i + 1)
                                                     .arg(mask.color.name())
                                                     .arg(mask.rect.width())
                                                     .arg(mask.rect.height())
                                                     .arg(mask.data.size())
                                                     .arg(mask.enabled ? QString() : tr(" · выключен")),
                                                 m_masks);
                item->setData(Qt::UserRole, i);
                item->setSizeHint(QSize(210, 54));
            }
            if (current >= 0 && current < m_masks->count())
                m_masks->setCurrentRow(current, QItemSelectionModel::NoUpdate);
            for (int index : selection)
                if (auto *item = m_masks->item(index))
                    item->setSelected(true);
            m_partCanvas->setSelectedMasks(selectedMasks());
            m_merge->setText(tr("Объединить маски"));
            m_merge->setIcon(QIcon());
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
    m_hint->setText(
        detail
            ? tr("Перетащите строки для порядка init. Объединение жестом использует цвет последней "
                 "захваченной маски.")
            : tr("Черновик · линейки — разрезы, стык — линия, область — крестик, двойной щелчок — маски."));
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
    int target = m_masks->currentRow();
    if (!selection.contains(target) && !selection.isEmpty())
        target = *std::max_element(selection.begin(), selection.end());
    if (m_session && m_part >= 0 && target >= 0 &&
        target < m_session->document.regions[m_part].masks.size() && selection.size() > 1)
    {
        const auto color = m_session->document.regions[m_part].masks[target].color;
        m_merge->setText(tr("Объединить в %1").arg(color.name()));
        m_merge->setIcon(colorIcon(color));
    }
    else
    {
        m_merge->setText(tr("Объединить маски"));
        m_merge->setIcon(QIcon());
    }
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
                           saveHistory();
                           m_session->document.regions[m_part].masks = reordered;
                           refresh();
                       });
}
