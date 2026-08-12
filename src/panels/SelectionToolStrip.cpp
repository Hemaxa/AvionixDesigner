#include "SelectionToolStrip.h"

#include <QIcon>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

SelectionToolStrip::SelectionToolStrip(QWidget *parent) : BasePanel(parent)
{
    setPanelName("SelectionToolStrip");
    setFixedWidth(64);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 8, 4, 8);
    layout->setSpacing(8);

    m_undoButton = createActionButton(QStringLiteral(":/icons/icons/selection/undo.svg"), QStringLiteral("Отменить"), QStringLiteral("Ctrl+Z"));
    connect(m_undoButton, &QToolButton::clicked, this, &SelectionToolStrip::undoRequested);
    layout->addWidget(m_undoButton);

    m_redoButton = createActionButton(QStringLiteral(":/icons/icons/selection/redo.svg"), QStringLiteral("Повторить"), QStringLiteral("Ctrl+Y"));
    connect(m_redoButton, &QToolButton::clicked, this, &SelectionToolStrip::redoRequested);
    layout->addWidget(m_redoButton);

    auto *frontButton = createActionButton(QStringLiteral(":/icons/icons/selection/send-front.svg"), QStringLiteral("Переместить на передний план"), QStringLiteral("Ctrl+]"));
    connect(frontButton, &QToolButton::clicked, this, [this]() {
        emit alignRequested(SendToFront);
    });
    m_selectionButtons.append(frontButton);
    layout->addWidget(frontButton);

    auto *backButton = createActionButton(QStringLiteral(":/icons/icons/selection/send-back.svg"), QStringLiteral("Переместить на задний план"), QStringLiteral("Ctrl+["));
    connect(backButton, &QToolButton::clicked, this, [this]() {
        emit alignRequested(SendToBack);
    });
    m_selectionButtons.append(backButton);
    layout->addWidget(backButton);

    auto *groupButton = createActionButton(QStringLiteral(":/icons/icons/selection/group.svg"), QStringLiteral("Сгруппировать"), QStringLiteral("Ctrl+G"));
    connect(groupButton, &QToolButton::clicked, this, &SelectionToolStrip::groupRequested);
    m_selectionButtons.append(groupButton);
    layout->addWidget(groupButton);

    auto *ungroupButton = createActionButton(QStringLiteral(":/icons/icons/selection/ungroup.svg"), QStringLiteral("Разгруппировать"), QStringLiteral("Ctrl+Shift+G"));
    connect(ungroupButton, &QToolButton::clicked, this, &SelectionToolStrip::ungroupRequested);
    m_selectionButtons.append(ungroupButton);
    layout->addWidget(ungroupButton);

    struct ActionDef
    {
        QString iconPath;
        QString toolTip;
        QString shortcut;
        int id;
    };

    const QList<ActionDef> actions = {
        {QStringLiteral(":/icons/icons/selection/align-top.svg"), QStringLiteral("Выровнять по верхнему краю"), QStringLiteral("Ctrl+Alt+Up"), AlignTop},
        {QStringLiteral(":/icons/icons/selection/align-vcenter.svg"), QStringLiteral("Выровнять по вертикальному центру"), QStringLiteral("Ctrl+Alt+V"), AlignVCenter},
        {QStringLiteral(":/icons/icons/selection/align-bottom.svg"), QStringLiteral("Выровнять по нижнему краю"), QStringLiteral("Ctrl+Alt+Down"), AlignBottom},
        {QStringLiteral(":/icons/icons/selection/align-left.svg"), QStringLiteral("Выровнять по левому краю"), QStringLiteral("Ctrl+Alt+Left"), AlignLeft},
        {QStringLiteral(":/icons/icons/selection/align-hcenter.svg"), QStringLiteral("Выровнять по горизонтальному центру"), QStringLiteral("Ctrl+Alt+H"), AlignHCenter},
        {QStringLiteral(":/icons/icons/selection/align-right.svg"), QStringLiteral("Выровнять по правому краю"), QStringLiteral("Ctrl+Alt+Right"), AlignRight}
    };

    for (const auto &action : actions) {
        auto *button = createActionButton(action.iconPath, action.toolTip, action.shortcut);
        connect(button, &QToolButton::clicked, this, [this, id = action.id]() {
            emit alignRequested(id);
        });
        m_buttons.append(button);
        m_selectionButtons.append(button);
        layout->addWidget(button);
    }

    layout->addStretch();

    m_copyButton = createActionButton(QStringLiteral(":/icons/icons/selection/copy.svg"), QStringLiteral("Копировать"), QStringLiteral("Ctrl+C"));
    connect(m_copyButton, &QToolButton::clicked, this, &SelectionToolStrip::copyRequested);
    m_selectionButtons.append(m_copyButton);
    layout->addWidget(m_copyButton);

    m_pasteButton = createActionButton(QStringLiteral(":/icons/icons/selection/paste.svg"), QStringLiteral("Вставить"), QStringLiteral("Ctrl+V"));
    connect(m_pasteButton, &QToolButton::clicked, this, &SelectionToolStrip::pasteRequested);
    layout->addWidget(m_pasteButton);

    m_deleteButton = createActionButton(QStringLiteral(":/icons/icons/selection/delete.svg"), QStringLiteral("Удалить объект"), QStringLiteral("Delete / Backspace"));
    connect(m_deleteButton, &QToolButton::clicked, this, &SelectionToolStrip::deleteRequested);
    m_selectionButtons.append(m_deleteButton);
    layout->addWidget(m_deleteButton);

    setSelectionActive(false);
    setHistoryAvailable(false, false);
    setPasteAvailable(false);
}

void SelectionToolStrip::setSelectionActive(bool active)
{
    for (QToolButton *button : std::as_const(m_selectionButtons)) {
        button->setEnabled(active);
    }
}

void SelectionToolStrip::setHistoryAvailable(bool canUndo, bool canRedo)
{
    if (m_undoButton)
        m_undoButton->setEnabled(canUndo);
    if (m_redoButton)
        m_redoButton->setEnabled(canRedo);
}

void SelectionToolStrip::setPasteAvailable(bool canPaste)
{
    if (m_pasteButton)
        m_pasteButton->setEnabled(canPaste);
}

QToolButton* SelectionToolStrip::createActionButton(const QString &iconPath, const QString &toolTip, const QString &shortcutText)
{
    auto *button = new QToolButton(this);
    button->setObjectName("SelectionToolButton");
    button->setIcon(QIcon(iconPath));
    button->setToolTip(shortcutText.isEmpty() ? toolTip : QStringLiteral("%1 (%2)").arg(toolTip, shortcutText));
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}
