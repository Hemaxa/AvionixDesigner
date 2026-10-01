#include "ThreeDViewObject.h"

#include "BitParser.h"
#include "ProjectManager.h"

#include <QtMath>

ThreeDViewObject::ThreeDViewObject(QObject *parent)
    : BaseObject(parent),
      m_placeholderRenderer(QStringLiteral(":/icons/icons/objects/three-d-view-placeholder.svg"))
{
    setExportEnabled(true);
    setEditRestrictionHint(QStringLiteral("3D-вид закреплен у правой границы рабочей области."));
}

void ThreeDViewObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    if (schema.contains(QStringLiteral("enb"))) {
        const ParamInfo field = schema.value(QStringLiteral("enb"));
        setViewVisible(BitParser::extract(hexInit, field.offset, field.size) != 0);
    }
}

void ThreeDViewObject::draw(QPainter &painter)
{
    const QRectF rect = viewRect();
    if (rect.isEmpty())
        return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(QPen(QColor(QStringLiteral("#2d5667")), 1.5));
    painter.setBrush(QColor(QStringLiteral("#0d2029")));
    painter.drawRect(rect.adjusted(0.6, 0.6, -0.6, -0.6));

    const double iconSize = qBound(40.0, qMin(rect.width(), rect.height()) * 0.28, 220.0);
    const QRectF iconRect(rect.center().x() - iconSize * 0.5,
                          rect.center().y() - iconSize * 0.5,
                          iconSize, iconSize);
    if (m_placeholderRenderer.isValid())
        m_placeholderRenderer.render(&painter, iconRect);

    painter.restore();
}

QString ThreeDViewObject::getTypeName() const
{
    return QStringLiteral("ThreeDView");
}

QString ThreeDViewObject::getDisplayName() const
{
    return QStringLiteral("3D вид");
}

QList<QPair<QString, QString>> ThreeDViewObject::getProperties() const
{
    const QRectF rect = viewRect();
    return {
        {QStringLiteral("Положение"), QStringLiteral("правая граница")},
        {QStringLiteral("X"), QString::number(rect.x(), 'f', 1)},
        {QStringLiteral("Y"), QString::number(rect.y(), 'f', 1)},
        {QStringLiteral("Ширина"), QString::number(rect.width(), 'f', 1)},
        {QStringLiteral("Высота"), QString::number(rect.height(), 'f', 1)},
        {QStringLiteral("Экспорт"), QStringLiteral("layer3d")}
    };
}

QRectF ThreeDViewObject::getBoundingRect() const
{
    return viewRect();
}

bool ThreeDViewObject::contains(const QPointF &point) const
{
    Q_UNUSED(point);
    return false;
}

void ThreeDViewObject::moveBy(double dx, double dy)
{
    Q_UNUSED(dx);
    Q_UNUSED(dy);
}

void ThreeDViewObject::resizeBy(int edgeFlags, double dx, double dy)
{
    Q_UNUSED(edgeFlags);
    Q_UNUSED(dx);
    Q_UNUSED(dy);
}

bool ThreeDViewObject::setObjectProperty(const QString &name, const QString &value)
{
    Q_UNUSED(name);
    Q_UNUSED(value);
    setValidationMessage(QStringLiteral("3D-вид закреплен у правой границы и не редактируется вручную."));
    return false;
}

QMap<QString, quint32> ThreeDViewObject::serializeParams() const
{
    const QRectF rect = viewRect();
    return {
        {QStringLiteral("enb"), isViewVisible() ? 1u : 0u},
        {QStringLiteral("x0"), static_cast<quint32>(qBound(0, qRound(rect.x()), 4095))},
        {QStringLiteral("y0"), static_cast<quint32>(qBound(0, qRound(rect.y()), 4095))}
    };
}

QRectF ThreeDViewObject::viewRect() const
{
    auto *project = ProjectManager::instance();
    const double canvasW = qMax(1, project->getCanvasWidth());
    const double canvasH = qMax(1, project->getCanvasHeight());
    const double side = qMin(canvasW, canvasH);
    return QRectF(canvasW - side, 0.0, side, canvasH);
}
