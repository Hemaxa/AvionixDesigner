// StaticImageCanvas — рисование, разрезы, стыки и объединение жестом мыши.
#include "AppearanceManager.h"
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
int rulerSize() { return qRound(AppearanceManager::instance()->number("static.ruler")); }
int canvasMargin() { return qRound(AppearanceManager::instance()->number("static.margin")); }
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
    connect(AppearanceManager::instance(), &AppearanceManager::styleChanged, this, [this]() {
        setSelectedMasks(m_selectedMasks);
        update();
    });
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
    m_preview = document ? document->render(region, region >= 0) : QImage();
    m_analysis.clear();
    m_fragments.clear();
    m_exactCut = false;
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
    m_isolatedImage = {};
    if (changed || !document)
        m_before = {};
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
    m_isolatedImage = {};
    m_fragments.clear();
    const auto *document = m_erasing ? &m_eraseDraft : m_document;
    if (document && m_region >= 0 && m_region < document->regions.size() && !indexes.isEmpty())
    {
        const auto &region = document->regions[m_region];
        m_maskHighlight = QImage(region.rect.size(), QImage::Format_ARGB32_Premultiplied);
        m_maskHighlight.fill(Qt::transparent);
        m_isolatedImage = m_maskHighlight;
        qint64 pixels = 0;
        int components = 0;
        QRect occupied;
        const QColor highlight = AppearanceManager::instance()->color("static.maskHighlight");
        const QRgb pixel = qPremultiply(highlight.rgba());
        for (int index : indexes)
        {
            if (index < 0 || index >= region.masks.size())
                continue;
            const auto &mask = region.masks[index];
            if (!m_erasing)
            {
                if (!m_analysis.contains(index))
                    m_analysis.insert(index, mask.analyze());
                const auto &analysis = m_analysis[index];
                m_fragments += analysis.fragments;
                pixels += analysis.pixels;
                components += analysis.components;
                occupied = occupied.united(analysis.bounds);
            }
            QPainter isolated(&m_isolatedImage);
            isolated.drawImage(mask.rect.topLeft() - region.rect.topLeft(), mask.image());
            isolated.end();
            for (int y = 0; y < mask.rect.height(); ++y)
                for (int x = 0; x < mask.rect.width(); ++x)
                    if (mask.data[y * mask.rect.width() + x] != 0)
                        reinterpret_cast<QRgb *>(m_maskHighlight.scanLine(
                            y + mask.rect.y() - region.rect.y()))[x + mask.rect.x() - region.rect.x()] =
                            pixel;
        }
        if (!m_erasing)
            emit selectionInfoChanged(tr("%1 × %2 px · %3 пикселей\nФрагментов: %4 · мелких: %5")
                                          .arg(occupied.width())
                                          .arg(occupied.height())
                                          .arg(pixels)
                                          .arg(components)
                                          .arg(m_fragments.size()));
    }
    else
        emit selectionInfoChanged(QString());
    update();
}

void StaticImageCanvas::setBeforeImage(const QImage &image)
{
    m_before = m_region < 0 ? image : image.copy(contentRect());
    update();
}
void StaticImageCanvas::setShowBefore(bool before)
{
    if (m_showBefore == before)
        return;
    cancelGesture();
    m_showBefore = before;
    update();
}
void StaticImageCanvas::setIsolated(bool isolated)
{
    m_isolated = isolated;
    update();
}
void StaticImageCanvas::setHandTool(bool enabled)
{
    m_handTool = enabled;
    setCursor(enabled ? Qt::OpenHandCursor : Qt::ArrowCursor);
}
void StaticImageCanvas::setExtendTool(bool enabled)
{
    m_extendTool = enabled;
    m_extensionRegions.clear();
    update();
}
void StaticImageCanvas::setEraserTool(bool enabled)
{
    m_eraserTool = enabled;
    update();
}
void StaticImageCanvas::setBrushSize(int size)
{
    size = qBound(1, size, 64);
    if (m_brushSize == size)
        return;
    m_brushSize = size;
    emit brushSizeChanged(size);
    update();
}
void StaticImageCanvas::setExactCut(int region, Qt::Orientation orientation, int coordinate)
{
    m_exactCut = region >= 0;
    m_exactRegion = region;
    m_exactOrientation = orientation;
    m_exactCoordinate = coordinate;
    update();
}
void StaticImageCanvas::focusNextFragment()
{
    if (m_fragments.isEmpty())
        return;
    m_fragment = (m_fragment + 1) % m_fragments.size();
    const auto center = QRectF(m_fragments[m_fragment]).center();
    const double baseScale = imageTransform().m11() / m_zoom;
    m_zoom = qBound(.25, 24.0 / baseScale, 64.0);
    const QPointF target(rulerSize() + (width() - rulerSize()) / 2.0, rulerSize() + (height() - rulerSize()) / 2.0);
    m_pan += target - imageTransform().map(center);
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
    const double scale = qMax(.001, qMin((width() - rulerSize() - 2.0 * canvasMargin()) / rect.width(),
                                         (height() - rulerSize() - 2.0 * canvasMargin()) / rect.height()) *
                                        m_zoom);
    QTransform transform;
    transform.translate(rulerSize() + (width() - rulerSize() - rect.width() * scale) / 2.0 + m_pan.x(),
                        rulerSize() + (height() - rulerSize() - rect.height() * scale) / 2.0 + m_pan.y());
    transform.scale(scale, scale);
    transform.translate(-rect.x(), -rect.y());
    return transform;
}

QRectF StaticImageCanvas::horizontalRulerRect() const
{
    const auto box = imageTransform().mapRect(QRectF(contentRect()));
    const double left = qMax(double(rulerSize() + canvasMargin()), box.left());
    const double right = qMin(double(width() - canvasMargin()), box.right());
    return QRectF(left, qMax(double(canvasMargin()), box.top() - rulerSize()), qMax(0.0, right - left), rulerSize());
}

QRectF StaticImageCanvas::verticalRulerRect() const
{
    const auto box = imageTransform().mapRect(QRectF(contentRect()));
    const double top = qMax(double(rulerSize() + canvasMargin()), box.top());
    const double bottom = qMin(double(height() - canvasMargin()), box.bottom());
    return QRectF(qMax(double(canvasMargin()), box.left() - rulerSize()), top, rulerSize(), qMax(0.0, bottom - top));
}

QPointF StaticImageCanvas::toImage(const QPointF &point) const
{
    return imageTransform().inverted().map(point);
}

void StaticImageCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), AppearanceManager::instance()->color("canvas"));
    auto *appearance = AppearanceManager::instance();
    const QColor accent = appearance->color("accent"), line = appearance->color("borderStrong"), muted = appearance->color("muted");
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
    painter.setClipRect(rect().adjusted(rulerSize() + canvasMargin(), rulerSize() + canvasMargin(), -canvasMargin(), -canvasMargin()));
    painter.setTransform(transform);
    painter.fillRect(bounds, AppearanceManager::instance()->color("panel"));
    // Нейтральная шахматная подложка ограничена изображением, независимо от его пропорций.
    painter.save();
    painter.setClipRect(bounds, Qt::IntersectClip);
    const int tile = qMax(1, qCeil(appearance->number("static.checkerTile") / scale));
    const auto visible = transform.inverted().mapRect(QRectF(rect())).toAlignedRect().intersected(bounds);
    for (int y = (visible.top() / tile) * tile; y <= visible.bottom(); y += tile)
        for (int x = (visible.left() / tile) * tile; x <= visible.right(); x += tile)
            if (((x / tile) + (y / tile)) % 2 == 0)
                painter.fillRect(QRect(x, y, tile, tile), AppearanceManager::instance()->color("hover"));
    painter.restore();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const QImage &preview =
        m_showBefore
            ? m_before
            : (m_region >= 0 && m_isolated && !m_selectedMasks.isEmpty() ? m_isolatedImage : m_preview);
    painter.drawImage(bounds.topLeft(), preview);
    if (scale >= 10)
    {
        painter.setPen(AppearanceManager::instance()->pen("pen.thin", AppearanceManager::instance()->color("border"), 1 / scale));
        const QRect visible = transform.inverted()
                                  .mapRect(QRectF(rulerSize(), rulerSize(), width() - rulerSize(), height() - rulerSize()))
                                  .toAlignedRect()
                                  .intersected(bounds);
        for (int x = visible.x(); x <= visible.x() + visible.width(); ++x)
            painter.drawLine(QPointF(x, visible.top()), QPointF(x, visible.y() + visible.height()));
        for (int y = visible.y(); y <= visible.y() + visible.height(); ++y)
            painter.drawLine(QPointF(visible.left(), y), QPointF(visible.x() + visible.width(), y));
    }
    if (!m_showBefore && m_region < 0)
    {
        for (int i = 0; i < m_document->regions.size(); ++i)
        {
            const auto &region = m_document->regions[i];
            const bool active = i == m_hoverRegion || (m_cutPreview && i == m_cutRegion),
                       selected = m_stroke.contains(i);
            if (active || selected || region.excluded)
            {
                QColor tint = appearance->color(region.excluded ? "danger" : "accent");
                tint.setAlpha(qRound(appearance->number(selected ? "static.selectedOpacity"
                    : (region.excluded ? "static.excludedOpacity" : "static.hoverOpacity"))));
                painter.fillRect(region.rect, tint);
            }
            painter.setPen(appearance->pen(active ? "pen.medium" : "pen.thin", active || selected ? accent : line, 1 / scale));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(region.rect));
            if (region.excluded)
            {
                const auto center = QRectF(region.rect).center();
                const double arm = qMin(14.0 / scale, qMin(region.rect.width(), region.rect.height()) * .23);
                painter.setPen(AppearanceManager::instance()->pen("pen.emphasis", AppearanceManager::instance()->color("danger"), 1 / scale));
                painter.drawLine(center + QPointF(-arm, -arm), center + QPointF(arm, arm));
                painter.drawLine(center + QPointF(-arm, arm), center + QPointF(arm, -arm));
            }
        }
    }
    else if (!m_showBefore && m_region >= 0)
    {
        if (!m_maskHighlight.isNull() && (m_isolated || m_eraserTool))
            painter.drawImage(bounds.topLeft(), m_maskHighlight);
        for (int i : m_selectedMasks)
            if (i >= 0 && i < m_document->regions[m_region].masks.size())
            {
                const auto &mask = m_document->regions[m_region].masks[i];
                painter.setPen(AppearanceManager::instance()->pen("pen.selection", line, 1 / scale));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(QRectF(mask.rect));
                if (m_analysis.contains(i) && !m_analysis[i].bounds.isEmpty())
                {
                    painter.setPen(AppearanceManager::instance()->pen("pen.medium", accent, 1 / scale));
                    painter.drawRect(QRectF(m_analysis[i].bounds));
                }
            }
    }
    auto drawCut = [&](int region, Qt::Orientation orientation, int coordinate, const QColor &color)
    {
        if (region < 0 || region >= m_document->regions.size())
            return;
        const auto r = m_document->regions[region].rect;
        painter.setPen(AppearanceManager::instance()->pen("pen.cut", color, 1 / scale));
        painter.drawLine(orientation == Qt::Vertical
                             ? QLineF(coordinate, r.y(), coordinate, r.y() + r.height())
                             : QLineF(r.x(), coordinate, r.x() + r.width(), coordinate));
    };
    if (!m_showBefore && m_exactCut)
        drawCut(m_exactRegion, m_exactOrientation, m_exactCoordinate, AppearanceManager::instance()->color("accent"));
    if (!m_showBefore)
        for (int target : m_extensionRegions)
            drawCut(target, m_cutOrientation, m_cutCoordinate, AppearanceManager::instance()->color("accent"));
    if (!m_showBefore && m_eraserTool && m_region >= 0 && underMouse())
    {
        const auto pixel = toImage(m_lastMouse);
        const QRect brush(int(std::floor(pixel.x())) - m_brushSize / 2,
                          int(std::floor(pixel.y())) - m_brushSize / 2, m_brushSize, m_brushSize);
        painter.setPen(AppearanceManager::instance()->pen("pen.medium", AppearanceManager::instance()->color("danger"), 1 / scale));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(brush));
    }
    if (!m_showBefore && m_cutPreview && m_cutRegion >= 0 && m_cutRegion < m_document->regions.size())
    {
        const auto rect = m_document->regions[m_cutRegion].rect;
        painter.setPen(AppearanceManager::instance()->pen("pen.cut", accent, 1 / scale));
        const QLineF cut = m_cutOrientation == Qt::Vertical
                               ? QLineF(m_cutCoordinate, rect.y(), m_cutCoordinate, rect.y() + rect.height())
                               : QLineF(rect.x(), m_cutCoordinate, rect.x() + rect.width(), m_cutCoordinate);
        painter.drawLine(cut);
        painter.setBrush(accent);
        painter.drawEllipse(cut.p1(), appearance->number("static.cutEndpointRadius") / scale, appearance->number("static.cutEndpointRadius") / scale);
        painter.drawEllipse(cut.p2(), appearance->number("static.cutEndpointRadius") / scale, appearance->number("static.cutEndpointRadius") / scale);
    }
    if (!m_showBefore && !m_mergeBoundary.isEmpty())
    {
        painter.setPen(AppearanceManager::instance()->pen("pen.merge", accent, 1 / scale));
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
        painter.fillRect(track, AppearanceManager::instance()->color("panel"));
        painter.setPen(line);
        painter.drawRect(track.adjusted(0, 0, -1, -1));
        const int activeIndex = m_cutPreview ? m_cutRegion : m_hoverRegion;
        if (m_region < 0 && activeIndex >= 0 && activeIndex < m_document->regions.size())
        {
            const auto box = transform.mapRect(QRectF(m_document->regions[activeIndex].rect));
            const QColor tint = appearance->color("static.rulerSelection");
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
            painter.setPen(AppearanceManager::instance()->pen("pen.emphasis", accent));
            if (horizontal)
                painter.drawLine(QPointF(p.x(), track.top()), QPointF(p.x(), track.bottom()));
            else
                painter.drawLine(QPointF(track.left(), p.y()), QPointF(track.right(), p.y()));
        }
        painter.restore();
    };
    drawRuler(topRuler, true);
    drawRuler(leftRuler, false);
    // Markers use screen coordinates so corner pixels remain visible outside the image clip.
    if (!m_showBefore && !m_erasing)
    {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(AppearanceManager::instance()->pen("pen.medium", AppearanceManager::instance()->color("danger")));
        painter.setBrush(Qt::NoBrush);
        const auto viewport = rect().adjusted(rulerSize() + canvasMargin(), rulerSize() + canvasMargin(), -canvasMargin(), -canvasMargin());
        for (const auto &fragment : m_fragments)
        {
            const auto center = transform.map(QRectF(fragment).center());
            if (!viewport.contains(center.toPoint()))
                continue;
            const double radius = qMax(6.0, qMax(fragment.width(), fragment.height()) * scale / 2.0 + 2.0);
            painter.drawEllipse(center, radius, radius);
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
    m_extensionRegions.clear();
    m_hoverRegion = -1;
    unsetCursor();
    if (m_handTool || m_spaceHeld)
        setCursor(Qt::OpenHandCursor);
    if (!m_document || m_region >= 0 || !m_editable || m_showBefore || m_handTool || m_spaceHeld)
    {
        update();
        return;
    }
    const QPointF imagePoint = toImage(point);
    const bool inImage =
        rect().adjusted(rulerSize() + canvasMargin(), rulerSize() + canvasMargin(), -canvasMargin(), -canvasMargin()).contains(point.toPoint());
    if (inImage)
    {
        m_hoverRegion = m_document->regionAt(imagePoint);
        if (m_hoverRegion >= 0)
            m_activeRegion = m_hoverRegion;
    }
    const bool verticalRuler = horizontalRulerRect().contains(point);
    const bool horizontalRuler = verticalRulerRect().contains(point);
    if (m_extendTool)
    {
        findExtension(imagePoint);
        setCursor(Qt::CrossCursor);
        update();
        return;
    }
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
        }
    }
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
}

void StaticImageCanvas::cancelGesture()
{
    m_clickTimer.stop();
    m_cutDragging = m_stroking = m_panning = m_cutPreview = m_extendPreview = false;
    const bool wasErasing = m_erasing;
    m_erasing = false;
    if (wasErasing && m_document)
    {
        m_preview = m_document->render(m_region, m_region >= 0);
        setSelectedMasks(m_selectedMasks);
    }
    m_eraseDraft = {};
    m_extensionRegions.clear();
    m_spaceHeld = false;
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

void StaticImageCanvas::findExtension(const QPointF &point)
{
    double nearest = 8.0 / imageTransform().m11();
    for (int a = 0; a < m_document->regions.size(); ++a)
        for (int b = a + 1; b < m_document->regions.size(); ++b)
        {
            const auto ra = m_document->regions[a].rect, rb = m_document->regions[b].rect;
            QLineF edge;
            Qt::Orientation orientation = Qt::Vertical;
            if (ra.x() + ra.width() == rb.x() || rb.x() + rb.width() == ra.x())
            {
                const int top = qMax(ra.y(), rb.y()),
                          bottom = qMin(ra.y() + ra.height(), rb.y() + rb.height());
                if (top < bottom)
                    edge = QLineF(qMax(ra.x(), rb.x()), top, qMax(ra.x(), rb.x()), bottom);
            }
            else if (ra.y() + ra.height() == rb.y() || rb.y() + rb.height() == ra.y())
            {
                const int left = qMax(ra.x(), rb.x()), right = qMin(ra.x() + ra.width(), rb.x() + rb.width());
                if (left < right)
                    edge = QLineF(left, qMax(ra.y(), rb.y()), right, qMax(ra.y(), rb.y()));
                orientation = Qt::Horizontal;
            }
            if (edge.isNull() || distanceToLine(point, edge) >= nearest)
                continue;
            QList<int> targets;
            const QPointF direction = orientation == Qt::Vertical ? QPointF(0, .01) : QPointF(.01, 0);
            const int coordinate = qRound(orientation == Qt::Vertical ? edge.x1() : edge.y1());
            for (const auto probe : {edge.p1() - direction, edge.p2() + direction})
            {
                const int region = m_document->regionAt(probe);
                if (region < 0 || targets.contains(region))
                    continue;
                const auto r = m_document->regions[region].rect;
                const int start = orientation == Qt::Vertical ? r.x() : r.y();
                const int length = orientation == Qt::Vertical ? r.width() : r.height();
                if (coordinate > start && coordinate < start + length)
                    targets.append(region);
            }
            if (targets.isEmpty())
                continue;
            nearest = distanceToLine(point, edge);
            m_extensionRegions = targets;
            m_cutOrientation = orientation;
            m_cutCoordinate = coordinate;
            m_boundaryLine = edge;
            m_mergeBoundary = {a, b};
        }
}

void StaticImageCanvas::eraseTo(const QPointF &point)
{
    m_eraseChanged |= m_eraseDraft.eraseMasks(m_region, sorted(m_selectedMasks), toImage(m_strokeLast),
                                              toImage(point), m_brushSize);
    m_strokeLast = point;
    m_preview = m_eraseDraft.render(m_region, true);
    setSelectedMasks(m_selectedMasks);
}

void StaticImageCanvas::mousePressEvent(QMouseEvent *event)
{
    m_skipRelease = false;
    m_lastMouse = m_pressPosition = event->position();
    m_pressedRegion = -1;
    setFocus(Qt::MouseFocusReason);
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton ||
        (event->button() == Qt::LeftButton && (m_handTool || m_spaceHeld)))
    {
        m_panning = true;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() != Qt::LeftButton || !m_document)
        return;
    if (m_showBefore)
        return;
    if (m_eraserTool && m_editable && m_region >= 0)
    {
        if (m_selectedMasks.isEmpty())
            return;
        m_erasing = true;
        m_eraseChanged = false;
        m_eraseDraft = *m_document;
        m_strokeLast = event->position();
        eraseTo(event->position());
        return;
    }
    updateHover(event->position());
    if (m_extendTool)
        return;
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
    if (m_erasing)
    {
        m_lastMouse = event->position();
        eraseTo(event->position());
        return;
    }
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
        setCursor(m_handTool || m_spaceHeld ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }
    if (event->button() != Qt::LeftButton || !m_document || m_showBefore)
        return;
    if (m_erasing)
    {
        eraseTo(event->position());
        m_erasing = false;
        const auto result = std::move(m_eraseDraft);
        m_eraseDraft = {};
        if (m_eraseChanged)
            emit eraseFinished(result);
        else
            setSelectedMasks(m_selectedMasks);
        return;
    }
    if (m_extendTool)
    {
        updateHover(event->position());
        if (!m_extensionRegions.isEmpty())
            emit cutsExtensionRequested(m_extensionRegions, m_cutOrientation, m_cutCoordinate);
        return;
    }
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
    if (m_document && !m_showBefore && !m_extendTool && !m_handTool && m_region < 0 &&
        event->button() == Qt::LeftButton)
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
    if (m_eraserTool && m_editable && !m_showBefore && event->modifiers().testFlag(Qt::ControlModifier))
    {
        const int delta = event->angleDelta().y() ? event->angleDelta().y() : event->pixelDelta().y();
        if (delta)
            setBrushSize(m_brushSize + (delta > 0 ? 1 : -1));
        event->accept();
        return;
    }
    if (!event->pixelDelta().isNull() && !event->modifiers().testFlag(Qt::ControlModifier))
    {
        m_pan += event->pixelDelta();
        update();
        event->accept();
        return;
    }
    const QPointF anchor = toImage(event->position());
    const double delta =
        event->angleDelta().y() != 0 ? event->angleDelta().y() : event->pixelDelta().y() * 3.0;
    m_zoom = qBound(.25, m_zoom * std::pow(1.0015, delta), 64.0);
    m_pan += event->position() - imageTransform().map(anchor);
    update();
    event->accept();
}

void StaticImageCanvas::leaveEvent(QEvent *)
{
    if (!m_cutDragging && !m_erasing)
    {
        m_extensionRegions.clear();
        m_cutPreview = false;
        m_hoverRegion = -1;
        m_mergeBoundary.clear();
        unsetCursor();
        update();
    }
}

void StaticImageCanvas::keyPressEvent(QKeyEvent *event)
{
    if (m_eraserTool && m_editable && !m_showBefore &&
        (event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_BracketRight))
    {
        const int step = event->modifiers().testFlag(Qt::ShiftModifier) ? 4 : 1;
        setBrushSize(m_brushSize + (event->key() == Qt::Key_BracketRight ? step : -step));
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
    {
        m_spaceHeld = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
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
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
    {
        m_spaceHeld = false;
        if (!m_panning)
            unsetCursor();
        event->accept();
        return;
    }
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
