//ObjectLibraryPanel - компактная библиотека добавления поддерживаемых объектов

#include "ObjectLibraryPanel.h"

#include "FpgaSchemaRegistry.h"
#include "FlowLayout.h"

#include <QDrag>
#include <QIcon>
#include <QMap>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSizePolicy>
#include <QToolButton>

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

QString toolTipWithShortcut(const QString &text, const QString &shortcutText)
{
    if (shortcutText.isEmpty())
        return text;
    return QStringLiteral("%1 (%2)").arg(text, shortcutText);
}

// Кнопка библиотеки передает идентификатор типа при перетаскивании на холст.
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

    m_rowLayout = createFlowPanel(this);
    createButtons();
}

void ObjectLibraryPanel::createButtons()
{
    QList<EditorObjectDescriptor> items;
    QMap<QString, QString> shortcutsByType;
    int shortcutIndex = 1;
    for (const auto &descriptor : FpgaSchemaRegistry::instance()->editorObjectCatalog()) {
        if (descriptor.creatableInMenu) {
            shortcutsByType.insert(descriptor.typeName, QStringLiteral("Alt+%1").arg(shortcutIndex));
            ++shortcutIndex;
        }
        if (descriptor.creatableInLibrary)
            items.append(descriptor);
    }

    for (int i = 0; i < items.size(); ++i) {
        const auto &item = items[i];
        QToolButton *card = createLibraryCard(item.typeName, item.iconPath, item.title);
        card->setToolTip(toolTipWithShortcut(item.title, shortcutsByType.value(item.typeName)));
        m_rowLayout->addWidget(card);
    }

    for (bool rotating : {false, true}) {
        const QString type = rotating ? QStringLiteral("__rotating_image__") : QStringLiteral("__static_image__");
        const QString title = rotating ? QStringLiteral("Добавить вращающееся изображение") : QStringLiteral("Добавить статическое изображение");
        const QString icon = rotating ? QStringLiteral(":/icons/icons/library/rotating-image.svg") : QStringLiteral(":/icons/icons/library/static-image.svg");
        auto *button = createLibraryCard(type, icon, title);
        button->setToolTip(toolTipWithShortcut(title, rotating ? QStringLiteral("Ctrl+Shift+I") : QStringLiteral("Ctrl+I")));
        m_rowLayout->addWidget(button);
    }
}

QToolButton* ObjectLibraryPanel::createLibraryCard(const QString &typeName, const QString &iconPath, const QString &title)
{
    QToolButton *button;
    if (!typeName.startsWith(QStringLiteral("__"))) {
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
        if (typeName.startsWith(QStringLiteral("__"))) {
            emit imageImportRequested(typeName == QStringLiteral("__rotating_image__"));
            return;
        }
        emit objectRequested(typeName);
    });

    return button;
}
