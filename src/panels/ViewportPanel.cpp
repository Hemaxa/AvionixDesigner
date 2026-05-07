#include "ViewportPanel.h"
#include "ProjectManager.h"
#include "AppearanceManager.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
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
    if (m_selectedIndex != index) {
        m_selectedIndex = index;
        update();
    }
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

void ViewportPanel::drawManipulators(QPainter &painter, const QRectF &rect, bool canRotate)
{
    painter.save();
    
    // Рамка выделения
    QPen dashPen(Qt::blue, 1, Qt::DashLine);
    dashPen.setCosmetic(true);
    painter.setPen(dashPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect);
    
    // Маркеры ресайза
    painter.setPen(Qt::blue);
    painter.setBrush(Qt::white);
    
    const double size = 6.0 / m_scale; // Фиксированный визуальный размер
    const double half = size / 2.0;
    
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
    
    // Маркер поворота
    if (canRotate) {
        painter.drawLine(QPointF(rect.center().x(), rect.top()), QPointF(rect.center().x(), rect.top() - size * 3));
        painter.setBrush(Qt::yellow);
        painter.drawEllipse(QPointF(rect.center().x(), rect.top() - size * 3), half, half);
    }
    
    painter.restore();
}

int ViewportPanel::hitTestManipulators(const QPointF &pos, const QRectF &rect, bool canRotate) const
{
    const double size = 6.0 / m_scale * 1.5; // Чуть больше зона клика
    const double half = size / 2.0;
    
    auto isHit = [pos, half](double x, double y) {
        return qAbs(pos.x() - x) <= half && qAbs(pos.y() - y) <= half;
    };
    
    if (canRotate && isHit(rect.center().x(), rect.top() - (6.0 / m_scale) * 3)) return 100; // Rotate enum

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
    
    //рисуем все объекты
    painter.setRenderHint(QPainter::Antialiasing);
    const auto &objects = pm->getObjects();
    for (int i = 0; i < objects.size(); ++i) {
        objects[i]->draw(painter);
        
        // Рисуем выделение
        if (i == m_selectedIndex) {
            QRectF rect = objects[i]->getBoundingRect();
            bool canRotate = objects[i]->getTypeName() == "RotationObject";
            drawManipulators(painter, rect, canRotate);
        }
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
            QRectF rect = obj->getBoundingRect();
            bool canRotate = obj->getTypeName() == "RotationObject";
            
            int hit = hitTestManipulators(canvasPos, rect, canRotate);
            if (hit == 100) {
                m_dragMode = Rotate;
                return;
            } else if (hit > 0) {
                m_dragMode = Resize;
                m_resizeEdgeFlags = hit;
                return;
            }
            
            if (obj->contains(canvasPos)) {
                m_dragMode = Move;
                return;
            }
        }
        
        // 2. Ищем объект под курсором (с конца списка, чтобы верхние объекты кликались первыми)
        m_dragMode = None;
        int newSelection = -1;
        for (int i = pm->getObjectCount() - 1; i >= 0; --i) {
            if (pm->getObjectAt(i)->contains(canvasPos)) {
                newSelection = i;
                m_dragMode = Move;
                break;
            }
        }
        
        if (m_selectedIndex != newSelection) {
            setSelectedIndex(newSelection);
            emit objectSelected(newSelection);
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
    
    QPointF canvasPos = mapToCanvas(event->pos());
    double dx = canvasPos.x() - m_lastMousePos.x();
    double dy = canvasPos.y() - m_lastMousePos.y();
    
    if (m_selectedIndex >= 0) {
        auto obj = ProjectManager::instance()->getObjectAt(m_selectedIndex);
        if (m_dragMode == Move) {
            obj->moveBy(dx, dy);
            emit objectChanged();
            update();
        } else if (m_dragMode == Resize) {
            obj->resizeBy(m_resizeEdgeFlags, dx, dy);
            emit objectChanged();
            update();
        } else if (m_dragMode == Rotate && obj->getTypeName() == "RotationObject") {
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
    m_dragMode = None;
}
