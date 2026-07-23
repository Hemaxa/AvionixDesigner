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

    m_undoButton = createActionButton(QStringLiteral(":/icons/icons/selection/undo.svg"), QStringLiteral("Отменить"));
    connect(m_undoButton, &QToolButton::clicked, this, &SelectionToolStrip::undoRequested);
    layout->addWidget(m_undoButton);

    m_redoButton = createActionButton(QStringLiteral(":/icons/icons/selection/redo.svg"), QStringLiteral("Повторить"));
    connect(m_redoButton, &QToolButton::clicked, this, &SelectionToolStrip::redoRequested);
    layout->addWidget(m_redoButton);

    auto *frontButton = createActionButton(QStringLiteral(":/icons/icons/selection/send-front.svg"), QStringLiteral("Переместить на передний план"));
    connect(frontButton, &QToolButton::clicked, this, [this]() {
        emit alignRequested(SendToFront);
    });
    m_selectionButtons.append(frontButton);
    layout->addWidget(frontButton);

    auto *backButton = createActionButton(QStringLiteral(":/icons/icons/selection/send-back.svg"), QStringLiteral("Переместить на задний план"));
    connect(backButton, &QToolButton::clicked, this, [this]() {
        emit alignRequested(SendToBack);
    });
    m_selectionButtons.append(backButton);
    layout->addWidget(backButton);

    struct ActionDef
    {
        QString iconPath;
        QString toolTip;
        int id;
    };

    const QList<ActionDef> actions = {
        {QStringLiteral(":/icons/icons/selection/align-top.svg"), QStringLiteral("Выровнять по верхнему краю"), AlignTop},
        {QStringLiteral(":/icons/icons/selection/align-vcenter.svg"), QStringLiteral("Выровнять по вертикальному центру"), AlignVCenter},
        {QStringLiteral(":/icons/icons/selection/align-bottom.svg"), QStringLiteral("Выровнять по нижнему краю"), AlignBottom},
        {QStringLiteral(":/icons/icons/selection/align-left.svg"), QStringLiteral("Выровнять по левому краю"), AlignLeft},
        {QStringLiteral(":/icons/icons/selection/align-hcenter.svg"), QStringLiteral("Выровнять по горизонтальному центру"), AlignHCenter},
        {QStringLiteral(":/icons/icons/selection/align-right.svg"), QStringLiteral("Выровнять по правому краю"), AlignRight}
    };

    for (const auto &action : actions) {
        auto *button = createActionButton(action.iconPath, action.toolTip);
        connect(button, &QToolButton::clicked, this, [this, id = action.id]() {
            emit alignRequested(id);
        });
        m_buttons.append(button);
        m_selectionButtons.append(button);
        layout->addWidget(button);
    }

    layout->addStretch();

    m_copyButton = createActionButton(QStringLiteral(":/icons/icons/selection/copy.svg"), QStringLiteral("Копировать"));
    connect(m_copyButton, &QToolButton::clicked, this, &SelectionToolStrip::copyRequested);
    m_selectionButtons.append(m_copyButton);
    layout->addWidget(m_copyButton);

    m_pasteButton = createActionButton(QStringLiteral(":/icons/icons/selection/paste.svg"), QStringLiteral("Вставить"));
    connect(m_pasteButton, &QToolButton::clicked, this, &SelectionToolStrip::pasteRequested);
    layout->addWidget(m_pasteButton);

    m_exportButton = createActionButton(QStringLiteral(":/icons/icons/selection/export-triangle.svg"), QStringLiteral("Экспорт кадра в XML"));
    connect(m_exportButton, &QToolButton::clicked, this, &SelectionToolStrip::exportRequested);
    layout->addWidget(m_exportButton);

    m_deleteButton = createActionButton(QStringLiteral(":/icons/icons/selection/delete.svg"), QStringLiteral("Удалить объект"));
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

QToolButton* SelectionToolStrip::createActionButton(const QString &iconPath, const QString &toolTip)
{
    auto *button = new QToolButton(this);
    button->setObjectName("SelectionToolButton");
    button->setIcon(QIcon(iconPath));
    button->setToolTip(toolTip);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}
