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
        layout->addWidget(button);
    }

    layout->addStretch();

    m_exportButton = createActionButton(QStringLiteral(":/icons/icons/selection/export-triangle.svg"), QStringLiteral("Экспорт кадра в XML"));
    connect(m_exportButton, &QToolButton::clicked, this, &SelectionToolStrip::exportRequested);
    layout->addWidget(m_exportButton);

    m_deleteButton = createActionButton(QStringLiteral(":/icons/icons/selection/delete.svg"), QStringLiteral("Удалить объект"));
    connect(m_deleteButton, &QToolButton::clicked, this, &SelectionToolStrip::deleteRequested);
    layout->addWidget(m_deleteButton);

    setSelectionActive(false);
}

void SelectionToolStrip::setSelectionActive(bool active)
{
    for (QToolButton *button : std::as_const(m_buttons)) {
        button->setEnabled(active);
    }
    if (m_deleteButton) {
        m_deleteButton->setEnabled(active);
    }
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
