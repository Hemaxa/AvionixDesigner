/**
 * @file ViewportWindow.cpp
 * @brief Реализация окна холста
 */

#include "ViewportWindow.h"
#include "../managers/ProjectManager.h"
#include <QPainter>
#include <QWheelEvent>

ViewportWindow::ViewportWindow(QWidget *parent)
    : QWidget(parent)
    , m_scale(1.0)
    , m_offsetX(0)
    , m_offsetY(0)
{
    setMinimumSize(200, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded,
            this, QOverload<>::of(&QWidget::update));
}

void ViewportWindow::setScale(double scale)
{
    m_scale = qBound(0.1, scale, 10.0);
    update();
}

double ViewportWindow::getScale() const
{
    return m_scale;
}

void ViewportWindow::resetView()
{
    m_scale = 1.0;
    m_offsetX = 0;
    m_offsetY = 0;
    update();
}

void ViewportWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    auto pm = ProjectManager::instance();
    
    // Заливаем фон виджета темно-серым
    painter.fillRect(rect(), QColor(0x1a, 0x1a, 0x1a));
    
    int canvasW = pm->getCanvasWidth();
    int canvasH = pm->getCanvasHeight();
    
    // Вычисляем масштаб для вписывания холста в окно
    double scaleX = static_cast<double>(width()) / canvasW;
    double scaleY = static_cast<double>(height()) / canvasH;
    double fitScale = qMin(scaleX, scaleY) * 0.95;
    
    double totalScale = fitScale * m_scale;
    
    double offsetX = (width() - canvasW * totalScale) / 2.0 + m_offsetX;
    double offsetY = (height() - canvasH * totalScale) / 2.0 + m_offsetY;
    
    painter.save();
    painter.translate(offsetX, offsetY);
    painter.scale(totalScale, totalScale);
    
    // Рисуем холст
    painter.fillRect(0, 0, canvasW, canvasH, pm->getBackgroundColor());
    
    // Рисуем все объекты
    painter.setRenderHint(QPainter::Antialiasing);
    for (const auto &obj : pm->getObjects()) {
        obj->draw(painter);
    }
    
    painter.restore();
    
    // Рисуем рамку вокруг холста
    painter.setPen(QPen(QColor(0x5a, 0x5a, 0x5a), 1));
    painter.drawRect(QRectF(offsetX, offsetY, canvasW * totalScale, canvasH * totalScale));
}

void ViewportWindow::wheelEvent(QWheelEvent *event)
{
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
