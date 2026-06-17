#include "SelectionToolStrip.h"

#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

SelectionToolStrip::SelectionToolStrip(QWidget *parent) : BasePanel(parent)
{
    setPanelName("SelectionToolStrip");
    setFixedWidth(56);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 8, 4, 8);
    layout->setSpacing(8);

    struct ActionDef
    {
        QString label;
        QString toolTip;
        int id;
    };

    const QList<ActionDef> actions = {
        {QStringLiteral("T"), QStringLiteral("Выровнять по верхнему краю"), AlignTop},
        {QStringLiteral("VC"), QStringLiteral("Выровнять по вертикальному центру"), AlignVCenter},
        {QStringLiteral("B"), QStringLiteral("Выровнять по нижнему краю"), AlignBottom},
        {QStringLiteral("L"), QStringLiteral("Выровнять по левому краю"), AlignLeft},
        {QStringLiteral("HC"), QStringLiteral("Выровнять по горизонтальному центру"), AlignHCenter},
        {QStringLiteral("R"), QStringLiteral("Выровнять по правому краю"), AlignRight}
    };

    for (const auto &action : actions) {
        auto *button = createActionButton(action.label, action.toolTip);
        connect(button, &QToolButton::clicked, this, [this, id = action.id]() {
            emit alignRequested(id);
        });
        m_buttons.append(button);
        layout->addWidget(button);
    }

    layout->addStretch();

    m_deleteButton = createActionButton(QStringLiteral("X"), QStringLiteral("Удалить объект"));
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

QToolButton* SelectionToolStrip::createActionButton(const QString &label, const QString &toolTip)
{
    auto *button = new QToolButton(this);
    button->setObjectName("SelectionToolButton");
    button->setText(label);
    button->setToolTip(toolTip);
    button->setFixedSize(40, 40);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}
