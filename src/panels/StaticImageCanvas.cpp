// StaticImageCanvas — рисование, разрезы, стыки и объединение жестом мыши.
#include "StaticImageCanvas.h"
#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace
{
constexpr int ruler = 28;
double distanceToLine(const QPointF &point, const QLineF &line)
{
    const QPointF delta = line.p2() - line.p1();
    const double length2 = QPointF::dotProduct(delta, delta);
    if (!length2)
        return QLineF(point, line.p1()).length();
    const double t = qBound(0.0, QPointF::dotProduct(point - line.p1(), delta) / length2, 1.0);
    return QLineF(point, line.p1() + t * delta).length();
}
QList<int> sorted(const QSet<int> &set)
{
    auto indexes = set.values();
    std::sort(indexes.begin(), indexes.end());
    return indexes;
}
} // namespace

StaticImageCanvas::StaticImageCanvas(QWidget *parent) : QWidget(parent)
{
    setObjectName("StaticImageCanvas");
    setMinimumSize(240, 240);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_clickTimer.setSingleShot(true);
    m_clickTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_clickTimer, &QTimer::timeout, this,
            [this]
            {
                if (m_editable && m_document && m_clickRegion >= 0 &&
                    m_clickRegion < m_document->regions.size())
                    emit exclusionRequested(m_clickRegion);
            });
}

void StaticImageCanvas::setDocument(const StaticImageDocument *document, int region)
{
    const bool changed = document != m_document || region != m_region;
    m_clickTimer.stop();
    m_document = document;
    m_region = region;
    m_cutDragging = m_stroking = false;
    m_preview = document ? document->render(region, true) : QImage();
    if (changed)
    {
        m_activeRegion = document && !document->regions.isEmpty() ? 0 : -1;
        fit();
    }
    if (document && (m_activeRegion < 0 || m_activeRegion >= document->regions.size()))
        m_activeRegion = document->regions.isEmpty() ? -1 : 0;
    m_cutPreview = false;
    m_mergeBoundary.clear();
    m_selectedMasks.clear();
    m_maskHighlight = {};
    update();
}

void StaticImageCanvas::setEditable(bool editable)
{
    m_editable = editable;
    m_clickTimer.stop();
    update();
}
void StaticImageCanvas::fit()
{
    m_zoom = 1;
    m_pan = {};
    update();
}

void StaticImageCanvas::setSelectedMasks(const QSet<int> &indexes)
{
    m_selectedMasks = indexes;
    m_maskHighlight = {};
    if (m_document && m_region >= 0 && m_region < m_document->regions.size() && !indexes.isEmpty())
    {
        const auto &region = m_document->regions[m_region];
        m_maskHighlight = QImage(region.rect.size(), QImage::Format_ARGB32_Premultiplied);
        m_maskHighlight.fill(Qt::transparent);
        QColor highlight("#00c8f0");
        highlight.setAlpha(150);
        const QRgb pixel = qPremultiply(highlight.rgba());
        for (int index : indexes)
        {
            if (index < 0 || index >= region.masks.size())
                continue;
            const auto &mask = region.masks[index];
            for (int y = 0; y < mask.rect.height(); ++y)
                for (int x = 0; x < mask.rect.width(); ++x)
                    if (mask.data[y * mask.rect.width() + x] != 0)
                        reinterpret_cast<QRgb *>(m_maskHighlight.scanLine(
                            y + mask.rect.y() - region.rect.y()))[x + mask.rect.x() - region.rect.x()] =
                            pixel;
        }
    }
    update();
}

QRect StaticImageCanvas::contentRect() const
{
    if (!m_document)
        return {};
    return m_region < 0 ? QRect(QPoint(), m_document->size) : m_document->regions.value(m_region).rect;
}

QTransform StaticImageCanvas::imageTransform() const
{
    const auto rect = contentRect();
    if (rect.isEmpty())
        return {};
    const double scale =
        qMax(.001, qMin((width() - ruler - 28.0) / rect.width(), (height() - ruler - 28.0) / rect.height()) *
                       m_zoom);
    QTransform transform;
    transform.translate(ruler + (width() - ruler - rect.width() * scale) / 2.0 + m_pan.x(),
                        ruler + (height() - ruler - rect.height() * scale) / 2.0 + m_pan.y());
    transform.scale(scale, scale);
    transform.translate(-rect.x(), -rect.y());
    return transform;
}

QPointF StaticImageCanvas::toImage(const QPointF &point) const
{
    return imageTransform().inverted().map(point);
}

void StaticImageCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#0a0f14"));
    const QColor accent("#00c8f0"), line("#334656"), muted("#9fb3c1");
    painter.fillRect(QRect(0, 0, width(), ruler), QColor("#121922"));
    painter.fillRect(QRect(0, 0, ruler, height()), QColor("#121922"));
    if (!m_document || m_preview.isNull())
    {
        painter.setPen(muted);
        painter.drawText(rect().adjusted(30, 30, -30, -30), Qt::AlignCenter | Qt::TextWordWrap,
                         tr("Выберите статическое изображение в сцене"));
        return;
    }
    const auto transform = imageTransform();
    const double scale = transform.m11();
    const auto bounds = contentRect();
    painter.save();
    painter.setClipRect(QRect(ruler, ruler, width() - ruler, height() - ruler));
    painter.setTransform(transform);
    painter.fillRect(bounds, QColor("#17212b"));
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(bounds.topLeft(), m_preview);
    if (scale >= 10)
    {
        painter.setPen(QPen(QColor("#22303c"), 1 / scale));
        const QRect visible = transform.inverted()
                                  .mapRect(QRectF(ruler, ruler, width() - ruler, height() - ruler))
                                  .toAlignedRect()
                                  .intersected(bounds);
        for (int x = visible.x(); x <= visible.x() + visible.width(); ++x)
            painter.drawLine(QPointF(x, visible.top()), QPointF(x, visible.y() + visible.height()));
        for (int y = visible.y(); y <= visible.y() + visible.height(); ++y)
            painter.drawLine(QPointF(visible.left(), y), QPointF(visible.x() + visible.width(), y));
    }
    if (m_region < 0)
    {
        for (int i = 0; i < m_document->regions.size(); ++i)
        {
            const auto &region = m_document->regions[i];
            const bool active = i == m_activeRegion, selected = m_stroke.contains(i);
            if (active || selected || region.excluded)
            {
                QColor tint = accent;
                tint.setAlpha(selected ? 55 : (region.excluded ? 30 : 15));
                painter.fillRect(region.rect, tint);
            }
            painter.setPen(QPen(active || selected ? accent : line, (active ? 1.5 : 1) / scale));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(region.rect));
            if (region.excluded)
            {
                const auto center = QRectF(region.rect).center();
                const double arm = qMin(14.0 / scale, qMin(region.rect.width(), region.rect.height()) * .23);
                painter.setPen(QPen(accent, 2 / scale));
                painter.drawLine(center + QPointF(-arm, -arm), center + QPointF(arm, arm));
                painter.drawLine(center + QPointF(-arm, arm), center + QPointF(arm, -arm));
            }
        }
    }
    else
    {
        if (!m_maskHighlight.isNull())
            painter.drawImage(bounds.topLeft(), m_maskHighlight);
        for (int i : m_selectedMasks)
            if (i >= 0 && i < m_document->regions[m_region].masks.size())
            {
                const auto &mask = m_document->regions[m_region].masks[i];
                painter.setPen(QPen(accent, 1.5 / scale));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(QRectF(mask.rect));
            }
    }
    if (m_cutPreview && m_cutRegion >= 0 && m_cutRegion < m_document->regions.size())
    {
        const auto rect = m_document->regions[m_cutRegion].rect;
        painter.setPen(QPen(accent, 1.5 / scale, Qt::DashLine));
        const QLineF cut = m_cutOrientation == Qt::Vertical
                               ? QLineF(m_cutCoordinate, rect.y(), m_cutCoordinate, rect.y() + rect.height())
                               : QLineF(rect.x(), m_cutCoordinate, rect.x() + rect.width(), m_cutCoordinate);
        painter.drawLine(cut);
        painter.setBrush(accent);
        painter.drawEllipse(cut.p1(), 3 / scale, 3 / scale);
        painter.drawEllipse(cut.p2(), 3 / scale, 3 / scale);
    }
    if (!m_mergeBoundary.isEmpty())
    {
        painter.setPen(QPen(accent, 3 / scale, Qt::DashLine));
        painter.drawLine(m_boundaryLine);
    }
    painter.restore();

    painter.setPen(line);
    painter.drawLine(ruler, ruler, width(), ruler);
    painter.drawLine(ruler, ruler, ruler, height());
    if (m_region < 0 && m_activeRegion >= 0 && m_activeRegion < m_document->regions.size())
    {
        const auto active = m_document->regions[m_activeRegion].rect;
        const auto box = transform.mapRect(QRectF(active));
        QColor tint = accent;
        tint.setAlpha(45);
        painter.fillRect(QRectF(box.x(), 0, box.width(), ruler), tint);
        painter.fillRect(QRectF(0, box.y(), ruler, box.height()), tint);
        painter.setPen(QPen(accent, 2));
        for (double x : {box.x(), box.x() + box.width()})
            painter.drawLine(QPointF(x, 7), QPointF(x, ruler));
        for (double y : {box.y(), box.y() + box.height()})
            painter.drawLine(QPointF(7, y), QPointF(ruler, y));
    }
    QSet<int> xs, ys;
    for (const auto &region : m_document->regions)
    {
        xs << region.rect.x() << region.rect.x() + region.rect.width();
        ys << region.rect.y() << region.rect.y() + region.rect.height();
    }
    painter.setPen(muted);
    for (int x : xs)
    {
        const double px = transform.map(QPointF(x, 0)).x();
        if (px >= ruler && px < width())
        {
            painter.drawLine(QPointF(px, ruler - 6), QPointF(px, ruler));
            painter.drawText(QPointF(px + 3, 15), QString::number(x));
        }
    }
    for (int y : ys)
    {
        const double py = transform.map(QPointF(0, y)).y();
        if (py >= ruler && py < height())
        {
            painter.drawLine(QPointF(ruler - 6, py), QPointF(ruler, py));
            painter.save();
            painter.translate(16, py - 3);
            painter.rotate(-90);
            painter.drawText(QPointF(), QString::number(y));
            painter.restore();
        }
    }
    if (m_cutPreview)
    {
        painter.setPen(accent);
        if (m_cutOrientation == Qt::Vertical)
        {
            const double x = transform.map(QPointF(m_cutCoordinate, 0)).x();
            painter.drawLine(QPointF(x, 0), QPointF(x, ruler));
        }
        else
        {
            const double y = transform.map(QPointF(0, m_cutCoordinate)).y();
            painter.drawLine(QPointF(0, y), QPointF(ruler, y));
        }
    }
}

int StaticImageCanvas::maskAt(const QPointF &point) const
{
    if (!m_document || m_region < 0 || m_region >= m_document->regions.size())
        return -1;
    const QPoint pixel(int(std::floor(point.x())), int(std::floor(point.y())));
    const auto &masks = m_document->regions[m_region].masks;
    for (int i = 0; i < masks.size(); ++i)
    {
        const auto &mask = masks[i];
        if (!mask.enabled || !mask.rect.contains(pixel))
            continue;
        const auto position = pixel - mask.rect.topLeft();
        return mask.data[position.y() * mask.rect.width() + position.x()] != 0 ? i : -1;
    }
    return -1;
}

void StaticImageCanvas::updateHover(const QPointF &point)
{
    m_cutPreview = m_extendPreview = false;
    m_mergeBoundary.clear();
    if (!m_document || m_region >= 0 || !m_editable)
    {
        update();
        return;
    }
    const QPointF imagePoint = toImage(point);
    if (point.x() > ruler && point.y() > ruler)
    {
        const int region = m_document->regionAt(imagePoint);
        if (region >= 0)
            m_activeRegion = region;
    }
    const bool verticalRuler = point.y() < ruler && point.x() >= ruler;
    const bool horizontalRuler = point.x() < ruler && point.y() >= ruler;
    if (verticalRuler || horizontalRuler)
    {
        const int coordinate = qRound(verticalRuler ? imagePoint.x() : imagePoint.y());
        auto contains = [&](const QRect &r)
        {
            return verticalRuler ? coordinate > r.x() && coordinate < r.x() + r.width()
                                 : coordinate > r.y() && coordinate < r.y() + r.height();
        };
        if (m_activeRegion < 0 || !contains(m_document->regions[m_activeRegion].rect))
        {
            m_activeRegion = -1;
            for (int i = 0; i < m_document->regions.size(); ++i)
                if (contains(m_document->regions[i].rect))
                {
                    m_activeRegion = i;
                    break;
                }
        }
        if (m_activeRegion >= 0)
        {
            m_cutRegion = m_activeRegion;
            m_cutCoordinate = coordinate;
            m_cutOrientation = verticalRuler ? Qt::Vertical : Qt::Horizontal;
            m_cutPreview = true;
            emit hintChanged(tr("Потяните разрез: %1 = %2. Границы подсвечены на линейках.")
                                 .arg(verticalRuler ? "X" : "Y")
                                 .arg(coordinate));
        }
    }
    else if (!m_mergeTool && point.x() >= ruler && point.y() >= ruler && m_activeRegion >= 0)
    {
        const double tolerance = 7 / imageTransform().m11();
        const QRect target = m_document->regions[m_activeRegion].rect;
        // На свободной стороне Т-стыка — продолжение в соседнюю область.
        for (const auto &other : m_document->regions)
        {
            const int right = other.rect.x() + other.rect.width(),
                      bottom = other.rect.y() + other.rect.height();
            for (const QPoint corner : {other.rect.topLeft(), QPoint(right, other.rect.y()),
                                        QPoint(other.rect.x(), bottom), QPoint(right, bottom)})
            {
                if (QLineF(imagePoint, corner).length() > tolerance)
                    continue;
                if ((corner.x() == target.x() || corner.x() == target.x() + target.width()) &&
                    corner.y() > target.y() && corner.y() < target.y() + target.height())
                {
                    m_cutOrientation = Qt::Horizontal;
                    m_cutCoordinate = corner.y();
                    m_cutPreview = m_extendPreview = true;
                }
                else if ((corner.y() == target.y() || corner.y() == target.y() + target.height()) &&
                         corner.x() > target.x() && corner.x() < target.x() + target.width())
                {
                    m_cutOrientation = Qt::Vertical;
                    m_cutCoordinate = corner.x();
                    m_cutPreview = m_extendPreview = true;
                }
                if (m_extendPreview)
                    break;
            }
            if (m_extendPreview)
                break;
        }
        if (m_extendPreview)
        {
            m_cutRegion = m_activeRegion;
            emit hintChanged(tr("Нажмите на стык, чтобы продолжить разрез в подсвеченную область."));
        }
        else
        {
            double nearest = tolerance;
            for (int a = 0; a < m_document->regions.size(); ++a)
                for (int b = a + 1; b < m_document->regions.size(); ++b)
                {
                    const auto ra = m_document->regions[a].rect, rb = m_document->regions[b].rect;
                    const auto united = ra.united(rb);
                    if (qint64(united.width()) * united.height() !=
                        qint64(ra.width()) * ra.height() + qint64(rb.width()) * rb.height())
                        continue;
                    QLineF edge;
                    if (ra.x() + ra.width() == rb.x() || rb.x() + rb.width() == ra.x())
                    {
                        const int x = qMax(ra.x(), rb.x());
                        edge = QLineF(x, united.y(), x, united.y() + united.height());
                    }
                    else if (ra.y() + ra.height() == rb.y() || rb.y() + rb.height() == ra.y())
                    {
                        const int y = qMax(ra.y(), rb.y());
                        edge = QLineF(united.x(), y, united.x() + united.width(), y);
                    }
                    if (edge.isNull())
                        continue;
                    const double distance = distanceToLine(imagePoint, edge);
                    if (distance < nearest)
                    {
                        nearest = distance;
                        m_boundaryLine = edge;
                        m_mergeBoundary = {a, b};
                    }
                }
            emit hintChanged(m_mergeBoundary.isEmpty() ? tr("Щелчок — крестик. Двойной щелчок — маски части.")
                                                       : tr("Нажмите на границу, чтобы убрать разрез."));
        }
    }
    update();
}

void StaticImageCanvas::collectStroke(const QPointF &point)
{
    const int steps = qMax(1, int(QLineF(m_strokeLast, point).length() / 2));
    for (int i = 0; i <= steps; ++i)
    {
        const auto p = toImage(m_strokeLast + (point - m_strokeLast) * (double(i) / steps));
        const int index = m_region < 0 ? m_document->regionAt(p) : maskAt(p);
        if (index >= 0)
        {
            m_stroke.insert(index);
            m_strokeTarget = index;
        }
    }
    m_strokeLast = point;
    if (m_region >= 0)
    {
        m_selectedMasks = m_stroke;
        emit masksSelected(sorted(m_stroke), m_strokeTarget);
    }
    update();
}

void StaticImageCanvas::mousePressEvent(QMouseEvent *event)
{
    m_skipRelease = false;
    m_lastMouse = event->position();
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)
    {
        m_panning = true;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() != Qt::LeftButton || !m_document)
        return;
    updateHover(event->position());
    if (m_editable && m_cutPreview && !m_extendPreview)
    {
        m_cutDragging = true;
        return;
    }
    if (m_editable && m_mergeTool)
    {
        m_stroking = true;
        m_stroke.clear();
        m_strokeLast = event->position();
        collectStroke(event->position());
        return;
    }
    if (m_region >= 0)
    {
        const int mask = maskAt(toImage(event->position()));
        if (!event->modifiers().testFlag(Qt::ControlModifier))
            m_selectedMasks.clear();
        if (mask >= 0)
        {
            if (m_selectedMasks.contains(mask))
                m_selectedMasks.remove(mask);
            else
                m_selectedMasks.insert(mask);
        }
        emit masksSelected(sorted(m_selectedMasks), mask);
        update();
    }
}

void StaticImageCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning)
    {
        m_pan += event->position() - m_lastMouse;
        m_lastMouse = event->position();
        update();
        return;
    }
    if (m_cutDragging)
    {
        const auto p = toImage(event->position());
        const auto bounds = m_document->regions[m_cutRegion].rect;
        m_cutCoordinate = m_cutOrientation == Qt::Vertical
                              ? qBound(bounds.x() + 1, qRound(p.x()), bounds.x() + bounds.width() - 1)
                              : qBound(bounds.y() + 1, qRound(p.y()), bounds.y() + bounds.height() - 1);
        update();
        return;
    }
    if (m_stroking)
    {
        collectStroke(event->position());
        return;
    }
    updateHover(event->position());
}

void StaticImageCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_skipRelease)
    {
        m_skipRelease = false;
        return;
    }
    if (m_panning)
    {
        m_panning = false;
        unsetCursor();
        return;
    }
    if (event->button() != Qt::LeftButton || !m_document)
        return;
    if (m_cutDragging)
    {
        m_cutDragging = false;
        if (rect().contains(event->position().toPoint()) && event->position().x() >= ruler &&
            event->position().y() >= ruler)
            emit splitRequested(m_cutRegion, m_cutOrientation, m_cutCoordinate);
        return;
    }
    if (m_stroking)
    {
        collectStroke(event->position());
        m_stroking = false;
        if (m_region < 0)
            emit regionsMergeRequested(sorted(m_stroke));
        else if (m_stroke.size() > 1)
            emit masksMergeRequested(sorted(m_stroke), m_strokeTarget);
        m_stroke.clear();
        update();
        return;
    }
    if (m_editable && m_region < 0)
    {
        updateHover(event->position());
        if (m_extendPreview)
            emit splitRequested(m_cutRegion, m_cutOrientation, m_cutCoordinate);
        else if (!m_mergeBoundary.isEmpty())
            emit regionsMergeRequested(m_mergeBoundary);
        else
        {
            m_clickRegion = m_document->regionAt(toImage(event->position()));
            m_clickTimer.start(QApplication::doubleClickInterval());
        }
    }
}

void StaticImageCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    m_clickTimer.stop();
    m_skipRelease = true;
    if (m_document && m_region < 0 && event->button() == Qt::LeftButton)
    {
        const int index = m_document->regionAt(toImage(event->position()));
        if (index >= 0)
            emit regionOpened(index);
    }
}

void StaticImageCanvas::wheelEvent(QWheelEvent *event)
{
    if (!m_document)
        return;
    const QPointF anchor = toImage(event->position());
    m_zoom = qBound(.25, m_zoom * std::pow(1.0015, event->angleDelta().y()), 64.0);
    m_pan += event->position() - imageTransform().map(anchor);
    update();
    event->accept();
}

void StaticImageCanvas::leaveEvent(QEvent *)
{
    if (!m_cutDragging)
    {
        m_cutPreview = false;
        m_mergeBoundary.clear();
        update();
    }
}
