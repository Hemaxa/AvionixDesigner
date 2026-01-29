#include "CanvasWidget.h"
#include <QPainter>

CanvasWidget::CanvasWidget(QWidget *parent)
    : QWidget(parent)
    , m_backgroundColor(Qt::black)
    , m_canvasWidth(640)
    , m_canvasHeight(480)
{
    setMinimumSize(200, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void CanvasWidget::setShapes(const QList<QSharedPointer<CorelShape>> &shapes)
{
    m_shapes = shapes;
    update();
}

void CanvasWidget::setBackgroundColor(const QColor &color)
{
    m_backgroundColor = color;
    update();
}

void CanvasWidget::setCanvasSize(int width, int height)
{
    m_canvasWidth = width;
    m_canvasHeight = height;
    update();
}

void CanvasWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    
    // Заливаем фон виджета серым
    painter.fillRect(rect(), QColor(80, 80, 80));
    
    // Вычисляем масштаб и позицию для центрирования холста
    double scaleX = static_cast<double>(width()) / m_canvasWidth;
    double scaleY = static_cast<double>(height()) / m_canvasHeight;
    double scale = qMin(scaleX, scaleY) * 0.95; // 95% чтобы был отступ
    
    double offsetX = (width() - m_canvasWidth * scale) / 2.0;
    double offsetY = (height() - m_canvasHeight * scale) / 2.0;
    
    painter.save();
    painter.translate(offsetX, offsetY);
    painter.scale(scale, scale);
    
    // Рисуем холст (рабочую область из XML)
    painter.fillRect(0, 0, m_canvasWidth, m_canvasHeight, m_backgroundColor);
    
    // Рисуем все фигуры
    painter.setRenderHint(QPainter::Antialiasing);
    for (const auto &shape : m_shapes) {
        shape->draw(painter);
    }
    
    painter.restore();
    
    // Рисуем рамку вокруг холста
    painter.setPen(QPen(Qt::gray, 1));
    painter.drawRect(QRectF(offsetX, offsetY, m_canvasWidth * scale, m_canvasHeight * scale));
}
