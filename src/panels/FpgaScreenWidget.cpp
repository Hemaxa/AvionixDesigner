//FpgaScreenWidget - декоративная рамка экрана для результата симулятора ПЛИС

#include "FpgaScreenWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QFontMetrics>

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

void FpgaScreenWidget::setSimulationActive(bool active)
{
    if (m_simulationActive == active)
        return;
    m_simulationActive = active;
    update();
}

void FpgaScreenWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(14, 18, 24));

    const QRectF backing = backingRect();

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#22303c")));
    painter.drawRoundedRect(backing, 12, 12);

    if (m_image.isNull()) {
        painter.setBrush(QColor(8, 12, 16));
        painter.drawRoundedRect(backing.adjusted(1, 1, -1, -1), 10, 10);
    } else {
        QPainterPath clipPath;
        clipPath.addRoundedRect(backing, 12, 12);
        painter.save();
        painter.setClipPath(clipPath);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(backing, m_image);
        painter.restore();

        painter.setPen(QPen(QColor(255, 255, 255, 20), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(backing.adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
    }

    if (!m_simulationActive) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(5, 8, 11, 215));
        painter.drawRoundedRect(backing.adjusted(1, 1, -1, -1), 10, 10);

        const QRect messageRect = backing.adjusted(22, 18, -22, -18).toAlignedRect();
        QFont titleFont = painter.font();
        titleFont.setBold(true);
        titleFont.setPixelSize(messageRect.height() < 100 ? 17 : 20);
        QFont descriptionFont = painter.font();
        descriptionFont.setPixelSize(messageRect.height() < 100 ? 12 : 14);

        const QString title = QStringLiteral("Симулятор неактивен");
        const QString description = QStringLiteral("Для включения запустите симуляцию.");
        const int titleHeight = QFontMetrics(titleFont).height();
        const QRect descriptionBounds = QFontMetrics(descriptionFont).boundingRect(
            QRect(0, 0, messageRect.width(), messageRect.height()),
            Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, description);
        const int spacing = 8;
        const int contentHeight = titleHeight + spacing + descriptionBounds.height();
        const int contentTop = messageRect.center().y() - contentHeight / 2;

        painter.setFont(titleFont);
        painter.setPen(QColor(QStringLiteral("#edf7fb")));
        painter.drawText(QRect(messageRect.left(), contentTop, messageRect.width(), titleHeight),
                         Qt::AlignCenter, title);
        painter.setFont(descriptionFont);
        painter.setPen(QColor(QStringLiteral("#9fb3c1")));
        painter.drawText(QRect(messageRect.left(), contentTop + titleHeight + spacing,
                               messageRect.width(), descriptionBounds.height()),
                         Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, description);
        painter.restore();
    }
}

QRectF FpgaScreenWidget::backingRect() const
{
    const QRectF bounds = rect().adjusted(2, 2, -2, -2);
    if (bounds.isEmpty())
        return bounds;

    QSizeF contentSize = m_image.isNull()
        ? QSizeF(16.0, 9.0)
        : QSizeF(qMax(1, m_image.width()), qMax(1, m_image.height()));
    contentSize.scale(bounds.size(), Qt::KeepAspectRatio);

    const QPointF topLeft(
        bounds.left() + (bounds.width() - contentSize.width()) / 2.0,
        bounds.top() + (bounds.height() - contentSize.height()) / 2.0
    );
    return QRectF(topLeft, contentSize);
}
