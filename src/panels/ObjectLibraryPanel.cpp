#include "ObjectLibraryPanel.h"

#include "FpgaSchemaRegistry.h"

#include <QDrag>
#include <QHBoxLayout>
#include <QIcon>
#include <QMimeData>
#include <QMouseEvent>
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

class DraggableToolButton : public QToolButton
{
public:
    DraggableToolButton(const QString &typeName, QWidget *parent = nullptr)
        : QToolButton(parent), m_typeName(typeName) {}

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            m_dragStartPos = event->pos();
        QToolButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton))
            return;

        if ((event->pos() - m_dragStartPos).manhattanLength() < 10)
            return;

        auto *drag = new QDrag(this);
        auto *mimeData = new QMimeData();
        mimeData->setData("application/x-avionix-object", m_typeName.toUtf8());
        drag->setMimeData(mimeData);

        // Создаём иконку перетаскивания из иконки кнопки
        QPixmap pixmap = icon().pixmap(QSize(48, 48));
        if (!pixmap.isNull())
            drag->setPixmap(pixmap);

        drag->exec(Qt::CopyAction);
    }

private:
    QString m_typeName;
    QPoint m_dragStartPos;
};
}

ObjectLibraryPanel::ObjectLibraryPanel(QWidget *parent) : BasePanel(parent)
{
    setPanelName("ObjectLibraryPanel");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    createButtons();
    mainLayout->addStretch(1);
    mainLayout->addLayout(m_rowLayout);
    mainLayout->addStretch(1);
}

void ObjectLibraryPanel::createButtons()
{
    m_rowLayout = new QHBoxLayout();
    m_rowLayout->setSpacing(8);
    m_rowLayout->setAlignment(Qt::AlignVCenter);

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
        m_rowLayout->addWidget(card, 0, Qt::AlignVCenter);
    }

    QToolButton *imageButton = createLibraryCard(QStringLiteral("__image__"), QStringLiteral(":/icons/icons/library/import-image.svg"), QStringLiteral("Добавить изображение"));
    m_libraryCards.append(imageButton);
    m_rowLayout->addWidget(imageButton, 0, Qt::AlignVCenter);

    m_rowLayout->addStretch();
}

QToolButton* ObjectLibraryPanel::createLibraryCard(const QString &typeName, const QString &iconPath, const QString &title)
{
    QToolButton *button;
    if (typeName != QStringLiteral("__image__")) {
        button = new DraggableToolButton(typeName, this);
    } else {
        button = new QToolButton(this);
    }
    button->setObjectName("LibraryCard");
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setToolTip(title);
    button->setStatusTip(title);

    QIcon icon(iconPath);
    if (icon.isNull()) {
        const QString glyph = title.isEmpty() ? "?" : title.left(1).toUpper();
        icon = createPlaceholderIcon(glyph);
    }
    button->setIcon(icon);

    connect(button, &QToolButton::clicked, this, [this, typeName]() {
        if (typeName == QStringLiteral("__image__")) {
            emit imageImportRequested();
            return;
        }
        emit objectRequested(typeName);
    });

    return button;
}
