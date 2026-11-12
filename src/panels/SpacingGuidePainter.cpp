#include "SpacingGuidePainter.h"

#include <QFontMetricsF>
#include <QLocale>
#include <QLineF>
#include <QPainter>
#include <algorithm>

void SpacingGuidePainter::draw(QPainter &painter, const QList<SpacingGuide> &guides,
                              const QColor &color, const QColor &background)
{
    if (guides.isEmpty()) return;
    painter.save();
    const auto transform = painter.worldTransform();
    painter.resetTransform();
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 1.0);
    painter.setPen(pen);
    const QFontMetricsF metrics(painter.font());
    QList<QRectF> labels;
    const QRectF viewport = painter.viewport().adjusted(4, 4, -4, -4);
    for (const auto &guide : guides) {
        const auto before = transform.mapRect(guide.before);
        const auto after = transform.mapRect(guide.after);
        const bool horizontal = guide.orientation == Qt::Horizontal;
        const double cross = horizontal
            ? (qMax(before.top(), after.top()) + qMin(before.bottom(), after.bottom())) / 2.0
            : (qMax(before.left(), after.left()) + qMin(before.right(), after.right())) / 2.0;
        const QPointF from = horizontal ? QPointF(before.right(), cross) : QPointF(cross, before.bottom());
        const QPointF to = horizontal ? QPointF(after.left(), cross) : QPointF(cross, after.top());
        const QPointF along = horizontal ? QPointF(1, 0) : QPointF(0, 1);
        const QPointF across = horizontal ? QPointF(0, 1) : QPointF(1, 0);
        painter.setPen(pen);
        painter.drawLine(from, to);
        painter.drawLine(from - across * 4, from + across * 4);
        painter.drawLine(to - across * 4, to + across * 4);
        // В узком промежутке стрелки разворачиваются наружу.
        const double arrow = QLineF(from, to).length() >= 14.0 ? 4.0 : -4.0;
        for (double sign : {-1.0, 1.0}) {
            painter.drawLine(from, from + along * arrow + across * (sign * 3));
            painter.drawLine(to, to - along * arrow + across * (sign * 3));
        }

        QString value = QLocale().toString(qMax(0.0, guide.distance()), 'f', 2);
        const auto decimal = QLocale().decimalPoint();
        while (value.endsWith(QLatin1Char('0'))) value.chop(1);
        if (value.endsWith(decimal)) value.chop(decimal.size());
        const QString label = value + QStringLiteral(" px");
        const QSizeF size(metrics.horizontalAdvance(label) + 10, metrics.height() + 4);
        const QPointF middle = (from + to) / 2.0;
        QRectF box(horizontal ? QPointF(middle.x() - size.width() / 2, middle.y() - size.height() - 6)
                              : QPointF(middle.x() + 7, middle.y() - size.height() / 2), size);
        if (horizontal && box.top() < viewport.top()) box.moveTop(middle.y() + 6);
        if (!horizontal && box.right() > viewport.right()) box.moveRight(middle.x() - 7);
        // Небольшие интервалы не должны накладывать подписи друг на друга.
        for (qsizetype attempt = 0; attempt < labels.size(); ++attempt) {
            const bool collision = std::any_of(labels.cbegin(), labels.cend(), [&](const QRectF &labelBox) {
                return labelBox.adjusted(-3,-3,3,3).intersects(box);
            });
            if (!collision) break;
            if (horizontal) box.translate(0, box.top() >= middle.y() ? size.height()+4 : -size.height()-4);
            else box.translate(box.right() <= middle.x() ? -size.width()-4 : size.width()+4, 0);
        }
        labels.append(box);
        painter.setBrush(background);
        painter.drawRoundedRect(box, 3, 3);
        painter.drawText(box, Qt::AlignCenter, label);
    }
    painter.restore();
}
