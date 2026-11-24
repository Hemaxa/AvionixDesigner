// StaticImageCanvas — рисование, разрезы, стыки и объединение жестом мыши.
#include "StaticImageCanvas.h"
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace
{
constexpr int ruler = 28;
constexpr int margin = 16;
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
    const int cutKey = changed ? 0 : m_cutKey;
    cancelGesture();
    m_cutKey = cutKey;
    m_hoverRegion = -1;
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
    if (!editable)
        cancelGesture();
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
    const double scale = qMax(.001, qMin((width() - ruler - 2.0 * margin) / rect.width(),
                                         (height() - ruler - 2.0 * margin) / rect.height()) *
                                        m_zoom);
    QTransform transform;
    transform.translate(ruler + (width() - ruler - rect.width() * scale) / 2.0 + m_pan.x(),
                        ruler + (height() - ruler - rect.height() * scale) / 2.0 + m_pan.y());
    transform.scale(scale, scale);
    transform.translate(-rect.x(), -rect.y());
    return transform;
}

QRectF StaticImageCanvas::horizontalRulerRect() const
{
    const auto box = imageTransform().mapRect(QRectF(contentRect()));
    const double left = qMax(double(ruler + margin), box.left());
    const double right = qMin(double(width() - margin), box.right());
    return QRectF(left, qMax(double(margin), box.top() - ruler), qMax(0.0, right - left), ruler);
}

QRectF StaticImageCanvas::verticalRulerRect() const
{
    const auto box = imageTransform().mapRect(QRectF(contentRect()));
    const double top = qMax(double(ruler + margin), box.top());
    const double bottom = qMin(double(height() - margin), box.bottom());
    return QRectF(qMax(double(margin), box.left() - ruler), top, ruler, qMax(0.0, bottom - top));
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
    painter.setClipRect(rect().adjusted(ruler + margin, ruler + margin, -margin, -margin));
    painter.setTransform(transform);
    painter.fillRect(bounds, QColor("#121922"));
    // Нейтральная шахматная подложка ограничена изображением, независимо от его пропорций.
    painter.save();
    painter.setClipRect(bounds, Qt::IntersectClip);
    const int tile = qMax(1, qCeil(12.0 / scale));
    const auto visible = transform.inverted().mapRect(QRectF(rect())).toAlignedRect().intersected(bounds);
    for (int y = (visible.top() / tile) * tile; y <= visible.bottom(); y += tile)
        for (int x = (visible.left() / tile) * tile; x <= visible.right(); x += tile)
            if (((x / tile) + (y / tile)) % 2 == 0)
                painter.fillRect(QRect(x, y, tile, tile), QColor("#17212b"));
    painter.restore();
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
            const bool active = i == m_hoverRegion || (m_cutPreview && i == m_cutRegion),
                       selected = m_stroke.contains(i);
            if (active || selected || region.excluded)
            {
                QColor tint(region.excluded ? "#ff5c7a" : "#00c8f0");
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
                painter.setPen(QPen(QColor("#ff5c7a"), 2 / scale));
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

    const auto topRuler = horizontalRulerRect(), leftRuler = verticalRulerRect();
    auto drawRuler = [&](const QRectF &track, bool horizontal)
    {
        if (track.isEmpty())
            return;
        painter.save();
        painter.setClipRect(track);
        painter.fillRect(track, QColor("#121922"));
        painter.setPen(line);
        painter.drawRect(track.adjusted(0, 0, -1, -1));
        const int activeIndex = m_cutPreview ? m_cutRegion : m_hoverRegion;
        if (m_region < 0 && activeIndex >= 0 && activeIndex < m_document->regions.size())
        {
            const auto box = transform.mapRect(QRectF(m_document->regions[activeIndex].rect));
            QColor tint = accent;
            tint.setAlpha(28);
            painter.fillRect(horizontal ? QRectF(box.x(), track.y(), box.width(), track.height())
                                        : QRectF(track.x(), box.y(), track.width(), box.height()),
                             tint);
        }
        // Шаг 1/2/5 выбирается по масштабу; подписи не накладываются друг на друга.
        const double desired = 64.0 / scale;
        double step = std::pow(10.0, std::floor(std::log10(qMax(1.0, desired))));
        if (step < desired)
            step *= 2;
        if (step < desired)
            step *= 2.5;
        if (step < desired)
            step *= 2;
        const int tick = qMax(1, qRound(step));
        const int first = horizontal ? bounds.x() : bounds.y();
        const int last = first + (horizontal ? bounds.width() : bounds.height());
        const auto imageVisible = transform.inverted().mapRect(track);
        const int visibleFirst =
            qMax(first, int(std::floor(horizontal ? imageVisible.left() : imageVisible.top())));
        const int visibleLast =
            qMin(last, int(std::ceil(horizontal ? imageVisible.right() : imageVisible.bottom())));
        painter.setPen(muted);
        for (int value = ((visibleFirst + tick - 1) / tick) * tick; value <= visibleLast; value += tick)
        {
            const auto p = transform.map(QPointF(value, value));
            const QString label = QString::number(value);
            if (horizontal)
            {
                painter.drawLine(QPointF(p.x(), track.bottom() - 5), QPointF(p.x(), track.bottom()));
                const double x =
                    qBound(track.left() + 3, p.x() + 4,
                           qMax(track.left() + 3,
                                track.right() - painter.fontMetrics().horizontalAdvance(label) - 3));
                painter.drawText(QPointF(x, track.top() + 15), label);
            }
            else
            {
                painter.drawLine(QPointF(track.right() - 5, p.y()), QPointF(track.right(), p.y()));
                painter.save();
                painter.translate(track.left() + 15, p.y() - 4);
                painter.rotate(-90);
                painter.drawText(QPointF(), label);
                painter.restore();
            }
        }
        if (m_cutPreview && horizontal == (m_cutOrientation == Qt::Vertical))
        {
            const auto p = transform.map(QPointF(m_cutCoordinate, m_cutCoordinate));
            painter.setPen(QPen(accent, 2));
            if (horizontal)
                painter.drawLine(QPointF(p.x(), track.top()), QPointF(p.x(), track.bottom()));
            else
                painter.drawLine(QPointF(track.left(), p.y()), QPointF(track.right(), p.y()));
        }
        painter.restore();
    };
    drawRuler(topRuler, true);
    drawRuler(leftRuler, false);
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
    m_hoverRegion = -1;
    unsetCursor();
    if (!m_document || m_region >= 0 || !m_editable)
    {
        update();
        return;
    }
    const QPointF imagePoint = toImage(point);
    const bool inImage =
        rect().adjusted(ruler + margin, ruler + margin, -margin, -margin).contains(point.toPoint());
    if (inImage)
    {
        m_hoverRegion = m_document->regionAt(imagePoint);
        if (m_hoverRegion >= 0)
            m_activeRegion = m_hoverRegion;
    }
    const bool verticalRuler = horizontalRulerRect().contains(point);
    const bool horizontalRuler = verticalRulerRect().contains(point);
    if (m_cutKey && m_hoverRegion >= 0)
    {
        m_cutRegion = m_hoverRegion;
        m_cutOrientation = m_cutKey == 1 ? Qt::Horizontal : Qt::Vertical;
        m_cutStartRegion = m_cutRegion;
        updateCut(point, true);
        setCursor(Qt::CrossCursor);
        update();
        return;
    }
    if (verticalRuler || horizontalRuler)
    {
        const int coordinate = qRound(verticalRuler ? imagePoint.x() : imagePoint.y());
        auto contains = [&](const QRect &r)
        {
            return verticalRuler ? coordinate > r.x() && coordinate < r.x() + r.width()
                                 : coordinate > r.y() && coordinate < r.y() + r.height();
        };
        if (m_activeRegion < 0 || m_activeRegion >= m_document->regions.size() ||
            !contains(m_document->regions[m_activeRegion].rect))
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
            setCursor(Qt::CrossCursor);
            emit hintChanged(tr("Потяните разрез в нужную область: %1 = %2. Shift — зафиксировать область.")
                                 .arg(verticalRuler ? "X" : "Y")
                                 .arg(coordinate));
        }
    }
    else if (!m_mergeTool && m_hoverRegion >= 0)
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
    else
        emit hintChanged(
            tr("Тяните разрез с линейки в нужную область. H / V + щелчок — разрез внутри области."));
    update();
}

void StaticImageCanvas::updateCut(const QPointF &point, bool lockRegion)
{
    const auto p = toImage(point);
    const int underCursor = m_document->regionAt(p);
    m_cutRegion = lockRegion ? m_cutStartRegion : underCursor;
    m_cutPreview = false;
    if (m_cutRegion < 0 || m_cutRegion >= m_document->regions.size())
        return;
    const auto bounds = m_document->regions[m_cutRegion].rect;
    const bool vertical = m_cutOrientation == Qt::Vertical;
    const int start = vertical ? bounds.x() : bounds.y();
    const int length = vertical ? bounds.width() : bounds.height();
    if (length < 2)
        return;
    m_cutCoordinate = qBound(start + 1, qRound(vertical ? p.x() : p.y()), start + length - 1);
    m_cutPreview = true;
    m_hoverRegion = m_cutRegion;
    emit hintChanged(tr("%1 · %2 = %3 px · Shift — зафиксировать область · Esc — отменить")
                         .arg(m_document->regions[m_cutRegion].name, vertical ? "X" : "Y")
                         .arg(m_cutCoordinate));
}

void StaticImageCanvas::cancelGesture()
{
    m_clickTimer.stop();
    m_cutDragging = m_stroking = m_panning = m_cutPreview = m_extendPreview = false;
    m_pressedRegion = -1;
    m_cutKey = 0;
    m_skipRelease = true;
    m_stroke.clear();
    m_mergeBoundary.clear();
    unsetCursor();
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
    m_lastMouse = m_pressPosition = event->position();
    m_pressedRegion = -1;
    setFocus(Qt::MouseFocusReason);
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
        m_clickTimer.stop();
        m_cutStartRegion = m_cutRegion;
        m_cutDragging = true;
        return;
    }
    if (m_cutKey)
        return;
    if (m_editable && m_mergeTool)
    {
        m_clickTimer.stop();
        m_stroking = true;
        m_stroke.clear();
        m_strokeLast = event->position();
        collectStroke(event->position());
        return;
    }
    m_pressedRegion = m_hoverRegion;
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
        updateCut(event->position(), event->modifiers().testFlag(Qt::ShiftModifier));
        update();
        return;
    }
    if (m_stroking)
    {
        collectStroke(event->position());
        return;
    }
    m_lastMouse = event->position();
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
        updateCut(event->position(), event->modifiers().testFlag(Qt::ShiftModifier));
        const bool valid = m_cutPreview && m_document->regionAt(toImage(event->position())) >= 0 &&
                           !horizontalRulerRect().contains(event->position()) &&
                           !verticalRulerRect().contains(event->position());
        m_cutPreview = false;
        if (valid)
            emit splitRequested(m_cutRegion, m_cutOrientation, m_cutCoordinate);
        updateHover(event->position());
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
        else if (m_pressedRegion >= 0 && m_pressedRegion == m_hoverRegion &&
                 QLineF(m_pressPosition, event->position()).length() < QApplication::startDragDistance())
        {
            m_clickRegion = m_hoverRegion;
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
        m_hoverRegion = -1;
        m_mergeBoundary.clear();
        unsetCursor();
        emit hintChanged(
            tr("Тяните разрез с линейки в нужную область. H / V + щелчок — разрез внутри области."));
        update();
    }
}

void StaticImageCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        cancelGesture();
        m_hoverRegion = -1;
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_0 && event->modifiers() == Qt::ControlModifier)
    {
        fit();
        event->accept();
        return;
    }
    if (m_editable && m_region < 0 && event->modifiers() == Qt::NoModifier &&
        (event->key() == Qt::Key_H || event->key() == Qt::Key_V))
    {
        m_cutKey = event->key() == Qt::Key_H ? 1 : 2;
        updateHover(m_lastMouse);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void StaticImageCanvas::keyReleaseEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat() && (event->key() == Qt::Key_H || event->key() == Qt::Key_V))
    {
        m_cutKey = 0;
        if (!m_cutDragging)
            updateHover(m_lastMouse);
    }
    QWidget::keyReleaseEvent(event);
}

void StaticImageCanvas::focusOutEvent(QFocusEvent *event)
{
    cancelGesture();
    m_hoverRegion = -1;
    update();
    QWidget::focusOutEvent(event);
}
