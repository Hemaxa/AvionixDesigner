#include "ObjectLibraryPanel.h"

#include "FpgaSchemaRegistry.h"

#include <QGridLayout>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QIcon createPlaceholderIcon(const QString &glyph)
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#243447"));
    painter.drawRoundedRect(QRectF(4, 4, 56, 56), 14, 14);

    painter.setPen(QColor("#D9E4F2"));
    QFont font("SF Pro Display", 22, QFont::DemiBold);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, glyph);

    return QIcon(pixmap);
}
}

ObjectLibraryPanel::ObjectLibraryPanel(QWidget *parent) : BasePanel(parent)
{
    setPanelName("ObjectLibraryPanel");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    createButtons();
    mainLayout->addLayout(m_gridLayout);
    mainLayout->addStretch();
}

void ObjectLibraryPanel::createButtons()
{
    m_gridLayout = new QGridLayout();
    m_gridLayout->setHorizontalSpacing(8);
    m_gridLayout->setVerticalSpacing(8);

    QList<EditorObjectDescriptor> items;
    for (const auto &descriptor : FpgaSchemaRegistry::instance()->editorObjectCatalog()) {
        if (descriptor.creatableInLibrary) {
            items.append(descriptor);
        }
    }

    for (int i = 0; i < items.size(); ++i) {
        const auto &item = items[i];
        QToolButton *card = createLibraryCard(item.typeName, item.iconPath, item.title);
        m_libraryCards.append(card);
        m_gridLayout->addWidget(card, i / 3, i % 3);
    }

    for (int col = 0; col < 3; ++col) {
        m_gridLayout->setColumnStretch(col, 1);
    }
}

QToolButton* ObjectLibraryPanel::createLibraryCard(const QString &typeName, const QString &iconPath, const QString &title)
{
    auto *button = new QToolButton(this);
    button->setObjectName("LibraryCard");
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    button->setMinimumSize(QSize(42, 42));
    button->setMaximumSize(QSize(48, 48));
    button->setIconSize(QSize(22, 22));
    button->setToolTip(title);
    button->setStatusTip(title);

    QIcon icon(iconPath);
    if (icon.isNull()) {
        const QString glyph = title.isEmpty() ? "?" : title.left(1).toUpper();
        icon = createPlaceholderIcon(glyph);
    }
    button->setIcon(icon);

    connect(button, &QToolButton::clicked, this, [this, typeName]() {
        emit objectRequested(typeName);
    });

    return button;
}
