#include "ThreeDViewObject.h"

#include "BitParser.h"
#include "AircraftPositionMapping.h"
#include "FpgaSchemaRegistry.h"
#include "ProjectManager.h"

#include <QFontMetricsF>
#include <QtMath>

namespace {
// Draw in a fixed design space, then scale the entire view together. This
// avoids a minimum font size becoming oversized when a small canvas is zoomed.
constexpr double kReferenceSide = 768.0;
}

ThreeDViewObject::ThreeDViewObject(QObject *parent)
    : BaseObject(parent),
      m_markerRenderer(QStringLiteral(":/icons/icons/objects/aircraft-position-dot.svg"))
{
    setExportEnabled(true);
    setEditRestrictionHint(QStringLiteral("3D-вид закреплен у правой границы рабочей области."));
}

void ThreeDViewObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    if (schema.contains(QStringLiteral("course")) && schema.contains(QStringLiteral("height"))) {
        m_initHex = hexInit.simplified().remove(QLatin1Char(' ')).rightJustified(38, QLatin1Char('0')).right(38).toUpper();
    } else if (schema.contains(QStringLiteral("enb"))) {
        // Older XML describes only the on-screen layer origin, not the aircraft.
        const ParamInfo field = schema.value(QStringLiteral("enb"));
        setViewVisible(BitParser::extract(hexInit, field.offset, field.size) != 0);
    }
}

void ThreeDViewObject::draw(QPainter &painter)
{
    const QRectF view = viewRect();
    if (view.isEmpty())
        return;

    const double scale = view.width() / kReferenceSide;
    const QRectF rect(QPointF(0, 0), view.size() / scale);
    painter.save();
    painter.translate(view.topLeft());
    painter.scale(scale, scale);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(QPen(QColor(QStringLiteral("#2d5667")), 1.5));
    painter.setBrush(QColor(QStringLiteral("#0d2029")));
    painter.drawRect(rect.adjusted(0.6, 0.6, -0.6, -0.6));

    painter.setClipRect(rect, Qt::IntersectClip);
    const QRectF worldMarker = aircraftMarkerRect();
    const QRectF marker((worldMarker.topLeft() - view.topLeft()) / scale,
                         worldMarker.size() / scale);
    if (m_markerRenderer.isValid())
        m_markerRenderer.render(&painter, marker);

    painter.setFont(annotationFont());
    const QFontMetricsF metrics(painter.font());
    const double availableWidth = rect.width() - 16.0;
    const QString coordinates = metrics.elidedText(
        AircraftPositionMapping::coordinateLabel(aircraftX(), aircraftY()),
        Qt::ElideRight, availableWidth - 12.0);
    const double labelWidth = qMin(availableWidth, metrics.horizontalAdvance(coordinates) + 12.0);
    const double labelHeight = metrics.height() + 8.0;
    const double left = qBound(rect.left() + 8.0, marker.center().x() - labelWidth * 0.5,
                               rect.right() - 8.0 - labelWidth);
    const QRectF label(left, marker.top() - labelHeight - 4.0, labelWidth, labelHeight);
    painter.setPen(QPen(QColor(QStringLiteral("#2d5667")), 1));
    painter.setBrush(QColor(QStringLiteral("#142c37")));
    painter.drawRoundedRect(label, 4, 4);
    painter.setPen(QColor(QStringLiteral("#edf7fb")));
    painter.drawText(label, Qt::AlignCenter, coordinates);

    painter.setPen(QColor(QStringLiteral("#9fb3c1")));
    painter.drawText(QRectF(rect.left() + 8.0, rect.bottom() - labelHeight - 4.0,
                            availableWidth, labelHeight), Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(AircraftPositionMapping::cellLabel(
                         aircraftX(), aircraftY(), ProjectManager::instance()->aircraftMapCellSize()),
                         Qt::ElideRight, availableWidth));

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
    QMap<QString, quint32> params;
    const auto schema = FpgaSchemaRegistry::instance()->buildSchema(QStringLiteral("layer3d"));
    for (const QString &name : {QStringLiteral("x"), QStringLiteral("y"),
                               QStringLiteral("course"), QStringLiteral("height")}) {
        const auto field = schema.value(name);
        params.insert(name, BitParser::extract(m_initHex, field.offset, field.size));
    }
    return params;
}

int ThreeDViewObject::aircraftX() const
{
    return BitParser::extractSigned(m_initHex, 17, 17);
}

int ThreeDViewObject::aircraftY() const
{
    return BitParser::extractSigned(m_initHex, 0, 17);
}

int ThreeDViewObject::aircraftAltitude() const
{
    return qRound(BitParser::extractSigned(m_initHex, 53, 18) / 4.0);
}

int ThreeDViewObject::aircraftCourse() const
{
    return qRound(BitParser::extract(m_initHex, 34, 19) * 360.0 / 524288.0) % 360;
}

bool ThreeDViewObject::setAircraftState(int x, int y, int altitude, int course)
{
    x = qBound(-65536, x, 65535);
    y = qBound(-65536, y, 65535);
    course = (course % 360 + 360) % 360;
    const QString previous = m_initHex;
    if (x != aircraftX())
        BitParser::inject(m_initHex, 17, 17, static_cast<quint32>(x));
    if (y != aircraftY())
        BitParser::inject(m_initHex, 0, 17, static_cast<quint32>(y));
    if (altitude != aircraftAltitude()) {
        // Match meters_to_height_raw, including its positive limit of 65535.
        const int rawHeight = qMin(65535, qBound(-32768, altitude, 16384) * 4);
        BitParser::inject(m_initHex, 53, 18, static_cast<quint32>(rawHeight));
    }
    if (course != aircraftCourse())
        BitParser::inject(m_initHex, 34, 19, qRound(course * 524288.0 / 360.0) % 524288);
    return previous != m_initHex;
}

QString ThreeDViewObject::serializeInit() const
{
    return m_initHex;
}

QRectF ThreeDViewObject::aircraftMarkerRect() const
{
    const QRectF view = viewRect();
    const double scale = view.width() / kReferenceSide;
    const QRectF rect(QPointF(0, 0), view.size() / scale);
    // Reserve proportional space above the marker and below the map so
    // neither label overlaps the dot, even at the four corners of a tile.
    const double textHeight = QFontMetricsF(annotationFont()).height();
    const QRectF inner = rect.adjusted(2, 2 + textHeight + 20.0,
                                      -2, -2 - textHeight - 16.0);
    const double size = 24.0;
    const QPointF center = AircraftPositionMapping::center(inner, aircraftX(), aircraftY(), size,
                                                          ProjectManager::instance()->aircraftMapCellSize());
    return QRectF(view.topLeft() + (center - QPointF(size * 0.5, size * 0.5)) * scale,
                  QSizeF(size * scale, size * scale));
}

QFont ThreeDViewObject::annotationFont() const
{
    QFont font;
    font.setPixelSize(20);
    font.setWeight(QFont::DemiBold);
    return font;
}

QRectF ThreeDViewObject::viewRect() const
{
    auto *project = ProjectManager::instance();
    const double canvasW = qMax(1, project->getCanvasWidth());
    const double canvasH = qMax(1, project->getCanvasHeight());
    const double side = qMin(canvasW, canvasH);
    return QRectF(canvasW - side, 0.0, side, canvasH);
}
