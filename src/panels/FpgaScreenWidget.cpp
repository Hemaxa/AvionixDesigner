// FpgaScreenWidget - декоративная рамка экрана для результата симулятора ПЛИС.

#include "AppearanceManager.h"
#include "FpgaScreenWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QFontMetrics>

FpgaScreenWidget::FpgaScreenWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("FpgaScreenWidget");
    setMinimumSize(260, 180);
    connect(AppearanceManager::instance(), &AppearanceManager::styleChanged, this, QOverload<>::of(&QWidget::update));
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
    painter.fillRect(rect(), AppearanceManager::instance()->color("simulator.background"));

    const QRectF backing = backingRect();
    const double radius = AppearanceManager::instance()->number("simulator.radius");
    const double innerRadius = AppearanceManager::instance()->number("simulator.innerRadius");

    painter.setPen(Qt::NoPen);
    painter.setBrush(AppearanceManager::instance()->color("simulator.frame"));
    painter.drawRoundedRect(backing, radius, radius);

    if (m_image.isNull()) {
        painter.setBrush(AppearanceManager::instance()->color("simulator.empty"));
        painter.drawRoundedRect(backing.adjusted(1, 1, -1, -1), innerRadius, innerRadius);
    } else {
        QPainterPath clipPath;
        clipPath.addRoundedRect(backing, radius, radius);
        painter.save();
        painter.setClipPath(clipPath);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(backing, m_image);
        painter.restore();

        painter.setPen(AppearanceManager::instance()->pen("pen.thin", AppearanceManager::instance()->color("simulator.edge")));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(backing.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    }

    if (!m_simulationActive) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(AppearanceManager::instance()->color("simulator.overlay"));
        painter.drawRoundedRect(backing.adjusted(1, 1, -1, -1), innerRadius, innerRadius);

        const QRect messageRect = backing.adjusted(22, 18, -22, -18).toAlignedRect();
        auto *appearance = AppearanceManager::instance();
        const bool compact = messageRect.height() < 100;
        const QFont titleFont = appearance->font(compact ? "simulator.compactTitle" : "simulator.title", painter.font());
        const QFont descriptionFont = appearance->font(compact ? "simulator.compactDescription" : "simulator.description", painter.font());

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
        painter.setPen(AppearanceManager::instance()->color("text"));
        painter.drawText(QRect(messageRect.left(), contentTop, messageRect.width(), titleHeight),
                         Qt::AlignCenter, title);
        painter.setFont(descriptionFont);
        painter.setPen(AppearanceManager::instance()->color("muted"));
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
