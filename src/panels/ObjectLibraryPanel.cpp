#include "ObjectLibraryPanel.h"

#include <QFrame>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
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
    painter.drawRoundedRect(QRectF(4, 4, 56, 56), 16, 16);

    painter.setPen(QColor("#D9E4F2"));
    QFont font("SF Pro Display", 24, QFont::DemiBold);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, glyph);

    return QIcon(pixmap);
}
}

ObjectLibraryPanel::ObjectLibraryPanel(QWidget *parent) : BasePanel(parent)
{
    setPanelName("ObjectLibraryPanel");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(14);

    auto *titleLabel = new QLabel("Поддерживаемые объекты", this);
    titleLabel->setObjectName("LibraryTitleLabel");
    mainLayout->addWidget(titleLabel);

    m_descriptionLabel = new QLabel(
        "В библиотеке оставлены только те модули, которые приложение уже умеет корректно читать и сохранять в XML.",
        this
    );
    m_descriptionLabel->setObjectName("LibraryDescriptionLabel");
    m_descriptionLabel->setWordWrap(true);
    mainLayout->addWidget(m_descriptionLabel);

    createButtons();
    mainLayout->addLayout(m_gridLayout);
    mainLayout->addStretch();
}

void ObjectLibraryPanel::createButtons()
{
    m_gridLayout = new QGridLayout();
    m_gridLayout->setHorizontalSpacing(12);
    m_gridLayout->setVerticalSpacing(12);

    struct LibraryItem {
        QString typeName;
        QString iconPath;
        QString title;
        QString subtitle;
    };

    const QList<LibraryItem> items = {
        {"aviagorizont", ":/icons/icons/library/aviahorizon.svg", "Авиагоризонт", "Линия горизонта, небо и земля"},
        {"rectangle", ":/icons/icons/library/rectangle.svg", "Прямоугольник", "Векторный примитив"},
        {"staticgroup", ":/icons/icons/library/staticgroup.svg", "Static", "Растровая группа состояний"},
        {"rotationobject", ":/icons/icons/library/rotationgroup.svg", "Rotation Group", "Поворотная растровая маска"}
    };

    for (int i = 0; i < items.size(); ++i) {
        const auto &item = items[i];
        QToolButton *card = createLibraryCard(item.typeName, item.iconPath, item.title, item.subtitle);
        m_libraryCards.append(card);
        m_gridLayout->addWidget(card, i, 0);
    }

    m_gridLayout->setColumnStretch(0, 1);
}

QToolButton* ObjectLibraryPanel::createLibraryCard(const QString &typeName, const QString &iconPath, const QString &title, const QString &subtitle)
{
    auto *button = new QToolButton(this);
    button->setObjectName("LibraryCard");
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setMinimumHeight(74);
    button->setIconSize(QSize(38, 38));
    button->setText(title + "\n" + subtitle);

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
