//FpgaScreenWidget - декоративная рамка экрана для результата симулятора ПЛИС

#include "FpgaScreenWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QSvgRenderer>

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
    painter.fillRect(rect(), QColor(14, 18, 24));

    const QRect inner = screenRectForImage();
    QSvgRenderer renderer(QStringLiteral(":/icons/icons/fpga/chip-screen.svg"));
    if (renderer.isValid())
        renderer.render(&painter, iconRect());

    if (m_image.isNull()) {
        painter.fillRect(inner, QColor(8, 12, 16));
        return;
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(inner, m_image);

    painter.setPen(QPen(QColor(255, 255, 255, 24), 1));
    painter.drawLine(inner.topLeft(), inner.topRight());
    painter.drawLine(inner.topLeft(), inner.bottomLeft());
}

QRect FpgaScreenWidget::iconRect() const
{
    const QRect bounds = rect().adjusted(8, 8, -8, -8);
    QSize iconSize(640, 420);
    iconSize.scale(bounds.size(), Qt::KeepAspectRatio);
    const QPoint topLeft(
        bounds.left() + (bounds.width() - iconSize.width()) / 2,
        bounds.top() + (bounds.height() - iconSize.height()) / 2
    );
    return QRect(topLeft, iconSize);
}

QRect FpgaScreenWidget::screenRectForImage() const
{
    const QRect chip = iconRect();
    const QRect bounds(
        chip.left() + qRound(chip.width() * 0.1625),
        chip.top() + qRound(chip.height() * 0.205),
        qRound(chip.width() * 0.675),
        qRound(chip.height() * 0.59)
    );
    if (m_image.isNull() || bounds.isEmpty())
        return bounds.adjusted(2, 2, -2, -2);

    QSize scaled = m_image.size();
    scaled.scale(bounds.size(), Qt::KeepAspectRatio);
    const QPoint topLeft(
        bounds.left() + (bounds.width() - scaled.width()) / 2,
        bounds.top() + (bounds.height() - scaled.height()) / 2
    );
    return QRect(topLeft, scaled);
}
