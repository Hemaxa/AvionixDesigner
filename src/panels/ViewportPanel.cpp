#include "ViewportPanel.h"
#include "ProjectManager.h"
#include "AppearanceManager.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QSet>
#include <QUrl>
#include <QVector>
#include "BaseObject.h"
#include <QtMath>

ViewportPanel::ViewportPanel(QWidget *parent)
    : BasePanel(parent)
    , m_scale(1.0)
    , m_offsetX(0)
    , m_offsetY(0)
{
    //устанавливаем имя для стилизации через QSS
    setPanelName("ViewportPanel");
    
    setMinimumSize(200, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAcceptDrops(true);
    
    //подключаемся к сигналу загрузки проекта
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, QOverload<>::of(&QWidget::update));
    
    //подключаемся к сигналу смены темы для перерисовки
    connect(AppearanceManager::instance(), &AppearanceManager::styleChanged, this, QOverload<>::of(&QWidget::update));
}

void ViewportPanel::setScale(double scale)
{
    //ограничиваем масштаб в разумных пределах
    m_scale = qBound(0.1, scale, 10.0);
    update();
}

double ViewportPanel::getScale() const
{
    return m_scale;
}

void ViewportPanel::resetView()
{
    //сбрасываем все параметры вида
    m_scale = 1.0;
    m_offsetX = 0;
    m_offsetY = 0;
    update();
}

void ViewportPanel::setSelectedIndex(int index)
{
    setSelectedIndexes(index >= 0 ? QList<int>{index} : QList<int>{});
}

void ViewportPanel::setSelectedIndexes(const QList<int> &indexes)
{
    QList<int> normalized;
    QSet<int> seen;
    const int objectCount = ProjectManager::instance()->getObjectCount();

    for (int index : indexes) {
        if (index < 0 || index >= objectCount || seen.contains(index))
            continue;
        seen.insert(index);
        normalized.append(index);
    }

    if (m_selectedIndexes == normalized)
        return;

    m_selectedIndexes = normalized;
    m_selectedIndex = normalized.size() == 1 ? normalized.first() : -1;
    update();
}

QPointF ViewportPanel::mapToCanvas(const QPoint &widgetPoint) const
{
    auto pm = ProjectManager::instance();
    double canvasW = pm->getCanvasWidth();
    double canvasH = pm->getCanvasHeight();
    
    double scaleX = static_cast<double>(width()) / canvasW;
    double scaleY = static_cast<double>(height()) / canvasH;
    double fitScale = qMin(scaleX, scaleY) * 0.95;
    double totalScale = fitScale * m_scale;
    
    double offsetX = (width() - canvasW * totalScale) / 2.0 + m_offsetX;
    double offsetY = (height() - canvasH * totalScale) / 2.0 + m_offsetY;
    
    return QPointF((widgetPoint.x() - offsetX) / totalScale, (widgetPoint.y() - offsetY) / totalScale);
}

void ViewportPanel::drawManipulators(QPainter &painter, const QRectF &rect, bool canResize, bool canRotate)
{
    painter.save();
    
    // Рамка выделения
    QPen dashPen(Qt::blue, 1, Qt::DashLine);
    dashPen.setCosmetic(true);
    painter.setPen(dashPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect);
    
    const double size = 6.0 / m_scale; // Фиксированный визуальный размер
    const double half = size / 2.0;

    if (canResize) {
        // Маркеры ресайза
        painter.setPen(Qt::blue);
        painter.setBrush(Qt::white);

        auto drawHandle = [&](double x, double y) {
            painter.drawRect(QRectF(x - half, y - half, size, size));
        };

        drawHandle(rect.left(), rect.top());
        drawHandle(rect.center().x(), rect.top());
        drawHandle(rect.right(), rect.top());
        drawHandle(rect.right(), rect.center().y());
        drawHandle(rect.right(), rect.bottom());
        drawHandle(rect.center().x(), rect.bottom());
        drawHandle(rect.left(), rect.bottom());
        drawHandle(rect.left(), rect.center().y());
    }
    
    // Маркер поворота
    if (canRotate) {
        painter.drawLine(QPointF(rect.center().x(), rect.top()), QPointF(rect.center().x(), rect.top() - size * 3));
        painter.setBrush(Qt::yellow);
        painter.drawEllipse(QPointF(rect.center().x(), rect.top() - size * 3), half, half);
    }
    
    painter.restore();
}

void ViewportPanel::drawGrid(QPainter &painter, int canvasW, int canvasH, double totalScale)
{
    const int baseStep = ProjectManager::instance()->gridStep();
    int step = baseStep;
    while (step * totalScale < 6.0 && step < 1000)
        step *= 2;

    QPen gridPen(ProjectManager::instance()->gridColor(), 1);
    gridPen.setCosmetic(true);
    painter.save();
    painter.setPen(gridPen);
    for (int x = step; x < canvasW; x += step)
        painter.drawLine(QPointF(x, 0), QPointF(x, canvasH));
    for (int y = step; y < canvasH; y += step)
        painter.drawLine(QPointF(0, y), QPointF(canvasW, y));
    painter.restore();
}

void ViewportPanel::drawSnapGuides(QPainter &painter, int canvasW, int canvasH)
{
    if (m_activeSnapGuides.isEmpty())
        return;

    auto *project = ProjectManager::instance();
    painter.save();
    for (const SnapGuide &guide : m_activeSnapGuides) {
        QColor color;
        switch (guide.kind) {
        case SnapGuideKind::Canvas:
            color = project->snapCanvasGuideColor();
            break;
        case SnapGuideKind::Grid:
            color = project->snapGridGuideColor();
            break;
        case SnapGuideKind::Object:
            color = project->snapObjectGuideColor();
            break;
        }

        QPen pen(color, 1.0, Qt::DashLine);
        pen.setCosmetic(true);
        painter.setPen(pen);
        if (guide.orientation == Qt::Vertical)
            painter.drawLine(QPointF(guide.position, 0), QPointF(guide.position, canvasH));
        else
            painter.drawLine(QPointF(0, guide.position), QPointF(canvasW, guide.position));
    }
    painter.restore();
}

ViewportPanel::SnapResult ViewportPanel::snappedMoveDelta(int objectIndex, const QRectF &originalRect, const QPointF &delta) const
{
    return snappedMoveDeltaForSelection(QList<int>{objectIndex}, originalRect, delta);
}

ViewportPanel::SnapResult ViewportPanel::snappedMoveDeltaForSelection(const QList<int> &objectIndexes, const QRectF &originalRect, const QPointF &delta) const
{
    auto *project = ProjectManager::instance();
    SnapResult result;
    result.delta = delta;
    constexpr double snapDistance = 6.0;
    const double gridStep = project->gridStep();
    QSet<int> selectedSet;
    for (int index : objectIndexes)
        selectedSet.insert(index);

    auto movedRect = [&originalRect, &result]() {
        return originalRect.translated(result.delta);
    };

    auto applyX = [&result](double target, double source, SnapGuideKind kind) {
        result.delta.setX(result.delta.x() + target - source);
        result.guides.append({kind, Qt::Vertical, target});
    };
    auto applyY = [&result](double target, double source, SnapGuideKind kind) {
        result.delta.setY(result.delta.y() + target - source);
        result.guides.append({kind, Qt::Horizontal, target});
    };

    if (project->snapToGrid()) {
        QRectF moved = movedRect();
        const QVector<double> sourceX = {moved.left(), moved.center().x(), moved.right()};

        double bestDistanceX = snapDistance + 1.0;
        double bestSourceX = 0.0;
        double bestTargetX = 0.0;
        for (double source : sourceX) {
            const double target = qRound(source / gridStep) * gridStep;
            const double distance = qAbs(source - target);
            if (distance <= snapDistance && distance < bestDistanceX) {
                bestDistanceX = distance;
                bestSourceX = source;
                bestTargetX = target;
            }
        }
        if (bestDistanceX <= snapDistance)
            applyX(bestTargetX, bestSourceX, SnapGuideKind::Grid);

        moved = movedRect();
        const QVector<double> sourceY = {moved.top(), moved.center().y(), moved.bottom()};
        double bestDistanceY = snapDistance + 1.0;
        double bestSourceY = 0.0;
        double bestTargetY = 0.0;
        for (double source : sourceY) {
            const double target = qRound(source / gridStep) * gridStep;
            const double distance = qAbs(source - target);
            if (distance <= snapDistance && distance < bestDistanceY) {
                bestDistanceY = distance;
                bestSourceY = source;
                bestTargetY = target;
            }
        }
        if (bestDistanceY <= snapDistance)
            applyY(bestTargetY, bestSourceY, SnapGuideKind::Grid);
    }

    QRectF moved = movedRect();
    if (project->snapToCanvas()) {
        if (qAbs(moved.left()) <= snapDistance)
            applyX(0.0, moved.left(), SnapGuideKind::Canvas);
        else if (qAbs(moved.right() - project->getCanvasWidth()) <= snapDistance)
            applyX(project->getCanvasWidth(), moved.right(), SnapGuideKind::Canvas);
        else if (qAbs(moved.center().x() - project->getCanvasWidth() / 2.0) <= snapDistance)
            applyX(project->getCanvasWidth() / 2.0, moved.center().x(), SnapGuideKind::Canvas);

        moved = movedRect();
        if (qAbs(moved.top()) <= snapDistance)
            applyY(0.0, moved.top(), SnapGuideKind::Canvas);
        else if (qAbs(moved.bottom() - project->getCanvasHeight()) <= snapDistance)
            applyY(project->getCanvasHeight(), moved.bottom(), SnapGuideKind::Canvas);
        else if (qAbs(moved.center().y() - project->getCanvasHeight() / 2.0) <= snapDistance)
            applyY(project->getCanvasHeight() / 2.0, moved.center().y(), SnapGuideKind::Canvas);
    }

    if (!project->snapToObjects())
        return result;

    moved = movedRect();
    const QVector<double> sourceX = {moved.left(), moved.center().x(), moved.right()};

    for (int i = 0; i < project->getObjectCount(); ++i) {
        if (selectedSet.contains(i))
            continue;

        const auto object = project->getObjectAt(i);
        if (!object || !object->isViewVisible())
            continue;

        const QRectF other = object->getBoundingRect();
        const QVector<double> targetX = {other.left(), other.center().x(), other.right()};
        const QVector<double> targetY = {other.top(), other.center().y(), other.bottom()};

        bool snappedX = false;
        for (double sx : sourceX) {
            for (double tx : targetX) {
                if (qAbs(sx - tx) <= snapDistance) {
                    applyX(tx, sx, SnapGuideKind::Object);
                    snappedX = true;
                    break;
                }
            }
            if (snappedX)
                break;
        }

        moved = movedRect();
        const QVector<double> sourceY = {moved.top(), moved.center().y(), moved.bottom()};
        bool snappedY = false;
        for (double sy : sourceY) {
            for (double ty : targetY) {
                if (qAbs(sy - ty) <= snapDistance) {
                    applyY(ty, sy, SnapGuideKind::Object);
                    snappedY = true;
                    break;
                }
            }
            if (snappedY)
                break;
        }

        if (snappedX || snappedY)
            break;
    }

    return result;
}

QRectF ViewportPanel::selectedObjectsRect() const
{
    QRectF bounds;
    bool hasBounds = false;
    auto *project = ProjectManager::instance();

    for (int index : m_selectedIndexes) {
        const auto object = project->getObjectAt(index);
        if (!object || !object->isViewVisible())
            continue;

        if (!hasBounds) {
            bounds = object->getBoundingRect();
            hasBounds = true;
        } else {
            bounds = bounds.united(object->getBoundingRect());
        }
    }

    return hasBounds ? bounds : QRectF();
}

bool ViewportPanel::selectedObjectContains(const QPointF &canvasPos) const
{
    auto *project = ProjectManager::instance();
    for (int i = m_selectedIndexes.size() - 1; i >= 0; --i) {
        const auto object = project->getObjectAt(m_selectedIndexes[i]);
        if (object && object->isViewVisible() && object->contains(canvasPos))
            return true;
    }
    return false;
}

void ViewportPanel::beginMoveDrag(const QPointF &canvasPos, const QRectF &bounds)
{
    m_dragMode = Move;
    m_dragStartCanvasPos = canvasPos;
    m_dragStartBounds = bounds;
    m_dragLastAppliedDelta = QPointF(0.0, 0.0);
    m_activeSnapGuides.clear();
}

int ViewportPanel::hitTestManipulators(const QPointF &pos, const QRectF &rect, bool canResize, bool canRotate) const
{
    const double size = 6.0 / m_scale * 1.5; // Чуть больше зона клика
    const double half = size / 2.0;
    
    auto isHit = [pos, half](double x, double y) {
        return qAbs(pos.x() - x) <= half && qAbs(pos.y() - y) <= half;
    };
    
    if (canRotate && isHit(rect.center().x(), rect.top() - (6.0 / m_scale) * 3)) return 100; // Rotate enum

    if (!canResize)
        return 0;

    if (isHit(rect.left(), rect.top())) return 1 | 4;         // Top-Left
    if (isHit(rect.center().x(), rect.top())) return 4;       // Top
    if (isHit(rect.right(), rect.top())) return 2 | 4;        // Top-Right
    if (isHit(rect.right(), rect.center().y())) return 2;     // Right
    if (isHit(rect.right(), rect.bottom())) return 2 | 8;     // Bottom-Right
    if (isHit(rect.center().x(), rect.bottom())) return 8;    // Bottom
    if (isHit(rect.left(), rect.bottom())) return 1 | 8;      // Bottom-Left
    if (isHit(rect.left(), rect.center().y())) return 1;      // Left
    
    return 0; // Нет попадания
}

void ViewportPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    auto pm = ProjectManager::instance();
    
    //заливаем фон виджета цветом из текущей темы
    painter.fillRect(rect(), AppearanceManager::instance()->getColor("viewport"));
    
    int canvasW = pm->getCanvasWidth();
    int canvasH = pm->getCanvasHeight();
    
    //вычисляем масштаб для вписывания холста в окно
    double scaleX = static_cast<double>(width()) / canvasW;
    double scaleY = static_cast<double>(height()) / canvasH;
    double fitScale = qMin(scaleX, scaleY) * 0.95;
    
    double totalScale = fitScale * m_scale;
    
    //центрируем холст
    double offsetX = (width() - canvasW * totalScale) / 2.0 + m_offsetX;
    double offsetY = (height() - canvasH * totalScale) / 2.0 + m_offsetY;
    
    painter.save();
    painter.translate(offsetX, offsetY);
    painter.scale(totalScale, totalScale);
    
    //рисуем холст
    painter.fillRect(0, 0, canvasW, canvasH, pm->getBackgroundColor());
    if (pm->showGrid())
        drawGrid(painter, canvasW, canvasH, totalScale);
    
    //рисуем все объекты
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const auto &objects = pm->getObjects();
    for (int i = objects.size() - 1; i >= 0; --i) {
        if (!objects[i]->isViewVisible())
            continue;

        objects[i]->draw(painter);
        
        // Рисуем выделение
        if (m_selectedIndexes.contains(i)) {
            QRectF rect = objects[i]->getBoundingRect();
            bool canRotate = objects[i]->supportsRotationHandle();
            const bool singleSelection = m_selectedIndexes.size() == 1;
            drawManipulators(painter, rect, singleSelection && objects[i]->canResize(), singleSelection && canRotate);
        }
    }

    if (m_selectedIndexes.size() > 1) {
        const QRectF groupRect = selectedObjectsRect();
        if (!groupRect.isNull())
            drawManipulators(painter, groupRect, false, false);
    }

    drawSnapGuides(painter, canvasW, canvasH);
    
    // Рисуем рамку выделения
    if (m_dragMode == MarqueeSelect) {
        painter.save();
        QColor marqueeColor = AppearanceManager::instance()->getColor("accent");
        marqueeColor.setAlpha(50);
        painter.setBrush(marqueeColor);
        painter.setPen(QPen(AppearanceManager::instance()->getColor("accent"), 1, Qt::DashLine));
        QRectF marqueeRect(m_marqueeStartPos, m_marqueeCurrentPos);
        painter.drawRect(marqueeRect.normalized());
        painter.restore();
    }
    
    painter.restore();
    
    //рисуем рамку вокруг холста
    painter.setPen(QPen(QColor(0x5a, 0x5a, 0x5a), 1));
    painter.drawRect(QRectF(offsetX, offsetY, canvasW * totalScale, canvasH * totalScale));
}

void ViewportPanel::wheelEvent(QWheelEvent *event)
{
    //масштабирование при зажатом Ctrl
    if (event->modifiers() & Qt::ControlModifier) {
        const double zoomStep = 0.1;
        if (event->angleDelta().y() > 0) {
            m_scale = qMin(m_scale * (1.0 + zoomStep), 10.0);
        } else {
            m_scale = qMax(m_scale * (1.0 - zoomStep), 0.1);
        }
        update();
        event->accept();
    } else {
        event->ignore();
    }
}

void ViewportPanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF canvasPos = mapToCanvas(event->pos());
        m_lastMousePos = canvasPos;
        auto pm = ProjectManager::instance();
        
        // 1. Проверяем клик по манипуляторам выделенного объекта
        if (m_selectedIndex >= 0 && m_selectedIndex < pm->getObjectCount()) {
            auto obj = pm->getObjectAt(m_selectedIndex);
            if (!obj->isViewVisible()) {
                setSelectedIndex(-1);
                emit objectSelected(-1);
                emit selectionChanged({});
                return;
            }
            QRectF rect = obj->getBoundingRect();
            bool canRotate = obj->supportsRotationHandle();
            
            int hit = hitTestManipulators(canvasPos, rect, obj->canResize(), canRotate);
            if (hit == 100) {
                m_dragMode = Rotate;
                return;
            } else if (hit > 0) {
                m_dragMode = Resize;
                m_resizeEdgeFlags = hit;
                return;
            }
            
            if (obj->contains(canvasPos)) {
                beginMoveDrag(canvasPos, obj->getBoundingRect());
                return;
            }
        }

        if (m_selectedIndexes.size() > 1 && selectedObjectContains(canvasPos)) {
            beginMoveDrag(canvasPos, selectedObjectsRect());
            return;
        }
        
        // 2. Ищем объект под курсором (верхние строки списка считаются верхними слоями)
        m_dragMode = None;
        int newSelection = -1;
        for (int i = 0; i < pm->getObjectCount(); ++i) {
            auto candidate = pm->getObjectAt(i);
            if (candidate->isViewVisible() && candidate->contains(canvasPos)) {
                newSelection = i;
                break;
            }
        }
        
        if (m_selectedIndex != newSelection) {
            setSelectedIndex(newSelection);
            emit objectSelected(newSelection);
            emit selectionChanged(m_selectedIndexes);
        }

        if (newSelection >= 0) {
            auto selected = pm->getObjectAt(newSelection);
            if (selected)
                beginMoveDrag(canvasPos, selected->getBoundingRect());
        }
        
        if (m_dragMode == None) {
            m_dragMode = MarqueeSelect;
            m_marqueeStartPos = canvasPos;
            m_marqueeCurrentPos = canvasPos;
        }
    } else if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_dragMode = Pan;
        m_lastMousePos = event->pos(); // Для пана используем экранные координаты
    }
}

void ViewportPanel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragMode == None) {
        // Меняем курсор при наведении на ручки
        // Можно реализовать позже
        return;
    }
    
    if (m_dragMode == Pan) {
        QPoint delta = event->pos() - m_lastMousePos.toPoint();
        m_offsetX += delta.x();
        m_offsetY += delta.y();
        m_lastMousePos = event->pos();
        update();
        return;
    }
    
    if (m_dragMode == MarqueeSelect) {
        m_marqueeCurrentPos = mapToCanvas(event->pos());
        update();
        return;
    }
    
    QPointF canvasPos = mapToCanvas(event->pos());
    double dx = canvasPos.x() - m_lastMousePos.x();
    double dy = canvasPos.y() - m_lastMousePos.y();
    
    if (m_dragMode == Move && m_selectedIndexes.size() > 1) {
        const QPointF desiredDelta = canvasPos - m_dragStartCanvasPos;
        const SnapResult snap = snappedMoveDeltaForSelection(m_selectedIndexes, m_dragStartBounds, desiredDelta);
        const QPointF applyDelta = snap.delta - m_dragLastAppliedDelta;
        for (int index : m_selectedIndexes) {
            auto object = ProjectManager::instance()->getObjectAt(index);
            if (object)
                object->moveBy(applyDelta.x(), applyDelta.y());
        }
        m_dragLastAppliedDelta = snap.delta;
        m_activeSnapGuides = snap.guides;
        emit objectChanged();
        update();
    } else if (m_selectedIndex >= 0) {
        auto obj = ProjectManager::instance()->getObjectAt(m_selectedIndex);
        if (m_dragMode == Move) {
            const QPointF desiredDelta = canvasPos - m_dragStartCanvasPos;
            const SnapResult snap = snappedMoveDelta(m_selectedIndex, m_dragStartBounds, desiredDelta);
            const QPointF applyDelta = snap.delta - m_dragLastAppliedDelta;
            obj->moveBy(applyDelta.x(), applyDelta.y());
            m_dragLastAppliedDelta = snap.delta;
            m_activeSnapGuides = snap.guides;
            emit objectChanged();
            update();
        } else if (m_dragMode == Resize) {
            m_activeSnapGuides.clear();
            obj->resizeBy(m_resizeEdgeFlags, dx, dy);
            emit objectChanged();
            update();
        } else if (m_dragMode == Rotate && obj->supportsRotationHandle()) {
            m_activeSnapGuides.clear();
            QRectF rect = obj->getBoundingRect();
            QPointF center = rect.center();
            double angleRad = qAtan2(canvasPos.y() - center.y(), canvasPos.x() - center.x());
            // Добавляем 90 градусов так как 0 градусов указывает направо, а ручка смотрит вверх
            double angleDeg = qRadiansToDegrees(angleRad) + 90.0;
            obj->setRotation(angleDeg);
            emit objectChanged();
            update();
        }
    }
    
    m_lastMousePos = canvasPos;
}

void ViewportPanel::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    
    if (m_dragMode == MarqueeSelect) {
        auto pm = ProjectManager::instance();
        QRectF marqueeRect = QRectF(m_marqueeStartPos, m_marqueeCurrentPos).normalized();
        
        QList<int> newSelection;
        for (int i = pm->getObjectCount() - 1; i >= 0; --i) {
            auto candidate = pm->getObjectAt(i);
            if (candidate->isViewVisible() && marqueeRect.intersects(candidate->getBoundingRect())) {
                newSelection.prepend(i);
            }
        }
        
        if (m_selectedIndexes != newSelection) {
            setSelectedIndexes(newSelection);
            emit objectSelected(m_selectedIndex);
            emit selectionChanged(m_selectedIndexes);
        }
        update();
    }
    
    m_dragMode = None;
    m_activeSnapGuides.clear();
    m_dragLastAppliedDelta = QPointF(0.0, 0.0);
    update();
}

void ViewportPanel::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-avionix-object")) {
        event->acceptProposedAction();
        return;
    }

    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    for (const QUrl &url : event->mimeData()->urls()) {
        const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
        if (suffix == QStringLiteral("png") || suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")
            || suffix == QStringLiteral("bmp") || suffix == QStringLiteral("svg")) {
            event->acceptProposedAction();
            return;
        }
    }

    event->ignore();
}

void ViewportPanel::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-avionix-object")) {
        const QString typeName = QString::fromUtf8(event->mimeData()->data("application/x-avionix-object"));
        const QPointF canvasPos = mapToCanvas(event->position().toPoint());
        emit objectDropped(typeName, canvasPos);
        event->acceptProposedAction();
        return;
    }

    for (const QUrl &url : event->mimeData()->urls()) {
        const QString fileName = url.toLocalFile();
        const QString suffix = QFileInfo(fileName).suffix().toLower();
        if (suffix == QStringLiteral("png") || suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")
            || suffix == QStringLiteral("bmp") || suffix == QStringLiteral("svg")) {
            emit imageDropped(fileName);
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}
