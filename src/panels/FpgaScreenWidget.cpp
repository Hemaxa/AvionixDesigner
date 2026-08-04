//FpgaScreenWidget - декоративная рамка экрана для результата симулятора ПЛИС

#include "FpgaScreenWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>

FpgaScreenWidget::FpgaScreenWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(260, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void FpgaScreenWidget::setFrameImage(const QImage &image)
{
    m_image = image;
    update();
}

void FpgaScreenWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(18, 22, 28));

    const QRect outer = rect().adjusted(14, 14, -14, -14);
    painter.setPen(QPen(QColor(78, 90, 104), 2));
    painter.setBrush(QColor(35, 42, 50));
    painter.drawRoundedRect(outer, 8, 8);

    const QRect inner = screenRectForImage();
    painter.setPen(QPen(QColor(10, 13, 16), 3));
    painter.setBrush(QColor(4, 6, 8));
    painter.drawRect(inner.adjusted(-2, -2, 2, 2));

    if (m_image.isNull()) {
        painter.fillRect(inner, QColor(8, 12, 16));
        return;
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(inner, m_image);

    painter.setPen(QPen(QColor(255, 255, 255, 28), 1));
    painter.drawLine(inner.topLeft(), inner.topRight());
    painter.drawLine(inner.topLeft(), inner.bottomLeft());
}

QRect FpgaScreenWidget::screenRectForImage() const
{
    const QRect bounds = rect().adjusted(28, 28, -28, -28);
    if (m_image.isNull() || bounds.isEmpty())
        return bounds;

    QSize scaled = m_image.size();
    scaled.scale(bounds.size(), Qt::KeepAspectRatio);
    const QPoint topLeft(
        bounds.left() + (bounds.width() - scaled.width()) / 2,
        bounds.top() + (bounds.height() - scaled.height()) / 2
    );
    return QRect(topLeft, scaled);
}
