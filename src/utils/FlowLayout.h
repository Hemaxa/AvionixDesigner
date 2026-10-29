// FlowLayout — перенос компактных органов управления на новые строки.
#pragma once

#include <QLayout>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent, int margin = 8, int gap = 6) : QLayout(parent) {
        setContentsMargins(margin, margin, margin, margin);
        setSpacing(gap);
    }
    ~FlowLayout() override { while (auto *item = takeAt(0)) delete item; }
    void addItem(QLayoutItem *item) override { m_items.append(item); }
    int count() const override { return m_items.size(); }
    QLayoutItem *itemAt(int index) const override { return m_items.value(index); }
    QLayoutItem *takeAt(int index) override {
        return index >= 0 && index < m_items.size() ? m_items.takeAt(index) : nullptr;
    }
    Qt::Orientations expandingDirections() const override { return Qt::Horizontal; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return arrange(QRect(0, 0, width, 0), false); }
    QSize minimumSize() const override {
        QSize result;
        for (auto *item : m_items) result = result.expandedTo(item->minimumSize());
        const auto margins = contentsMargins();
        return result + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    }
    QSize sizeHint() const override { return QSize(320, heightForWidth(320)); }
    void setGeometry(const QRect &rect) override { QLayout::setGeometry(rect); arrange(rect, true); }

private:
    int arrange(const QRect &rect, bool apply) const {
        const auto margins = contentsMargins();
        const QRect area = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
        int x = area.x(), y = area.y(), rowHeight = 0;
        for (auto *item : m_items) {
            if (item->isEmpty()) continue;
            const QSize size = item->sizeHint().expandedTo(item->minimumSize());
            if (rowHeight > 0 && x + size.width() > area.x() + area.width()) {
                x = area.x(); y += rowHeight + spacing(); rowHeight = 0;
            }
            if (apply) item->setGeometry(QRect(QPoint(x, y), size));
            x += size.width() + spacing();
            rowHeight = qMax(rowHeight, size.height());
        }
        return y + rowHeight - rect.y() + margins.bottom();
    }
    QList<QLayoutItem *> m_items;
};

// Прокрутка сохраняет доступ к кнопкам, если высоты плавающей панели не хватает.
class FlowScrollArea : public QScrollArea
{
public:
    explicit FlowScrollArea(QWidget *parent) : QScrollArea(parent) {
        setObjectName(QStringLiteral("ToolPanelScrollArea"));
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    QSize minimumSizeHint() const override { return QSize(110, 74); }
    QSize sizeHint() const override { return QSize(320, 96); }
protected:
    bool viewportEvent(QEvent *event) override {
        const bool result = QScrollArea::viewportEvent(event);
        if (event->type() == QEvent::Resize && widget() && widget()->layout())
            widget()->setMinimumHeight(widget()->layout()->heightForWidth(viewport()->width()));
        return result;
    }
};

// Общая оболочка трех панелей инструментов: библиотека, холст и передача ПЛИС.
inline FlowLayout *createFlowPanel(QWidget *panel)
{
    panel->setMinimumSize(110, 74);
    auto *scroll = new FlowScrollArea(panel);
    auto *body = new QWidget;
    auto *flow = new FlowLayout(body);
    scroll->setWidget(body);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);
    return flow;
}
