//ThreeDViewObject - область 3D-вида и положение самолета для ПЛИС

#include "ThreeDViewObject.h"

#include "BitParser.h"
#include "AircraftPositionMapping.h"
#include "FpgaSchemaRegistry.h"
#include "ProjectManager.h"

#include <QFontMetricsF>
#include <QtMath>

namespace {
// Сначала рисуем в фиксированной системе координат, затем масштабируем весь вид.
// Так подписи не становятся непропорционально крупными на маленьком холсте.
constexpr double kReferenceSide = 768.0;
}

ThreeDViewObject::ThreeDViewObject(QObject *parent)
    : BaseObject(parent),
      m_markerRenderer(QStringLiteral(":/icons/icons/objects/aircraft-position-dot.svg"))
{
    setExportEnabled(true);
    BitParser::inject(m_initHex, Layer3dFormat::screenYOffset, Layer3dFormat::screenYBits, Layer3dFormat::defaultScreenY);
    BitParser::inject(m_initHex, Layer3dFormat::heightOffset, Layer3dFormat::heightBits, Layer3dFormat::heightToRaw(Layer3dFormat::defaultHeight));
    BitParser::inject(m_initHex, Layer3dFormat::courseOffset, Layer3dFormat::courseBits, Layer3dFormat::courseToRaw(Layer3dFormat::defaultCourse));
    setEditRestrictionHint(QStringLiteral("3D-вид закреплен у границы холста. Сторона выбирается в параметре «Положение»."));
}

void ThreeDViewObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    if (schema.contains(QStringLiteral("course")) && schema.contains(QStringLiteral("height"))) {
        m_initHex = hexInit.simplified().remove(QLatin1Char(' '))
            .rightJustified(Layer3dFormat::hexLength, QLatin1Char('0'))
            .right(Layer3dFormat::hexLength).toUpper();
        // В контейнере из 38 HEX-символов два старших бита не входят в 150-битное слово.
        m_initHex[0] = QLatin1Char("0123"[m_initHex.left(1).toUInt(nullptr, 16) & 3]);
        const ParamSchema current = FpgaSchemaRegistry::instance()->buildSchema(QStringLiteral("layer3d"));
        const QString source = m_initHex;
        for (const QString &name : {QStringLiteral("x"), QStringLiteral("y"),
                                   QStringLiteral("course"), QStringLiteral("height"),
                                   QStringLiteral("yscreen")}) {
            const ParamInfo target = current.value(name);
            const ParamInfo original = schema.value(name, target);
            if (name == QStringLiteral("yscreen")) {
                BitParser::inject(m_initHex, target.offset, target.size,
                                  schema.contains(name) ? BitParser::extract(source, original.offset, original.size)
                                      : schema.contains("pitch") ? BitParser::extract(source, schema["pitch"].offset, schema["pitch"].size)
                                      : Layer3dFormat::defaultScreenY);
            } else if (name == QStringLiteral("x") || name == QStringLiteral("y")) {
                // Текущие координаты — беззнаковая фиксированная точка Q13.6.
                // Старая 17-битная раскладка хранила знаковые целые метры.
                const int raw = original.size == Layer3dFormat::coordinateBits
                    ? int(BitParser::extract(source, original.offset, original.size))
                    : qBound(Layer3dFormat::minPosition,
                             int(BitParser::extractSigned(source, original.offset, original.size)),
                             Layer3dFormat::maxPosition) * Layer3dFormat::coordinateUnitsPerPoint;
                BitParser::inject(m_initHex, target.offset, target.size,
                                  qMin(raw, Layer3dFormat::maxPosition * Layer3dFormat::coordinateUnitsPerPoint));
            } else if (name == QStringLiteral("course")) {
                BitParser::inject(m_initHex, target.offset, target.size,
                                  BitParser::extract(source, original.offset, original.size));
            } else {
                int value = BitParser::extractSigned(source, original.offset, original.size);
                if (name == QStringLiteral("height")) {
                    const int clamped = qBound(Layer3dFormat::minHeightRaw, value, Layer3dFormat::maxHeightRaw);
                    if (clamped != value)
                        emit ProjectManager::instance()->logMessage(QStringLiteral(
                            "Высота из старого XML ограничена диапазоном нового 17-битного поля layer3d."));
                    value = clamped;
                }
                BitParser::inject(m_initHex, target.offset, target.size, static_cast<quint32>(value));
            }
        }
    } else if (schema.contains(QStringLiteral("enb"))) {
        // В старом XML задана только экранная позиция слоя, а не положение самолета.
        const ParamInfo field = schema.value(QStringLiteral("enb"));
        setViewVisible(BitParser::extract(hexInit, field.offset, field.size) != 0);
        if (schema.contains("x0")) {
            const auto origin = schema.value("x0");
            m_leftAnchored = BitParser::extract(hexInit, origin.offset, origin.size) < 512;
        }
    }
}

void ThreeDViewObject::parseExtraData(const QDomElement &element)
{
    for (QDomNode node = element.firstChild(); !node.isNull(); node = node.nextSibling()) {
        if (node.isComment() && node.nodeValue().trimmed() == QStringLiteral("AvionixDesigner: 3D view left"))
            m_leftAnchored = true;
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

    painter.setPen(QPen(QColor(QStringLiteral("#22303c")), 1.5));
    painter.setBrush(QColor(QStringLiteral("#121922")));
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
    const double labelTop = marker.top() - labelHeight - 4.0;
    const QRectF label(left, labelTop < rect.top() + 4.0 ? marker.bottom() + 4.0 : labelTop,
                       labelWidth, labelHeight);
    painter.setPen(QPen(QColor(QStringLiteral("#22303c")), 1));
    painter.setBrush(QColor(QStringLiteral("#17212b")));
    painter.drawRoundedRect(label, 4, 4);
    painter.setPen(QColor(QStringLiteral("#edf7fb")));
    painter.drawText(label, Qt::AlignCenter, coordinates);

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
        {QStringLiteral("Положение"), m_leftAnchored ? QStringLiteral("Слева") : QStringLiteral("Справа")},
        {QStringLiteral("X"), QString::number(rect.x(), 'f', 1)},
        {QStringLiteral("Y"), QString::number(rect.y(), 'f', 1)},
        {QStringLiteral("Ширина"), QString::number(rect.width(), 'f', 1)},
        {QStringLiteral("Высота"), QString::number(rect.height(), 'f', 1)}
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
    if (name == QStringLiteral("Положение")) {
        if (value == QStringLiteral("Слева") || value == QStringLiteral("Справа")) {
            m_leftAnchored = value == QStringLiteral("Слева");
            return true;
        }
        setValidationMessage(QStringLiteral("Выберите положение «Слева» или «Справа»."));
        return false;
    }
    setValidationMessage(QStringLiteral("Размеры 3D-вида определяются размером холста."));
    return false;
}

QMap<QString, quint32> ThreeDViewObject::serializeParams() const
{
    QMap<QString, quint32> params;
    const auto schema = FpgaSchemaRegistry::instance()->buildSchema(QStringLiteral("layer3d"));
    for (const QString &name : {QStringLiteral("x"), QStringLiteral("y"),
                               QStringLiteral("course"), QStringLiteral("height"),
                               QStringLiteral("yscreen")}) {
        const auto field = schema.value(name);
        params.insert(name, BitParser::extract(m_initHex, field.offset, field.size));
    }
    return params;
}

double ThreeDViewObject::aircraftX() const
{
    return BitParser::extract(m_initHex, Layer3dFormat::xOffset, Layer3dFormat::coordinateBits)
        / double(Layer3dFormat::coordinateUnitsPerPoint);
}

double ThreeDViewObject::aircraftY() const
{
    return BitParser::extract(m_initHex, Layer3dFormat::yOffset, Layer3dFormat::coordinateBits)
        / double(Layer3dFormat::coordinateUnitsPerPoint);
}

double ThreeDViewObject::aircraftAltitude() const
{
    return BitParser::extractSigned(m_initHex, Layer3dFormat::heightOffset, Layer3dFormat::heightBits)
        / double(Layer3dFormat::heightUnitsPerMeter);
}

double ThreeDViewObject::aircraftCourse() const
{
    return BitParser::extract(m_initHex, Layer3dFormat::courseOffset, Layer3dFormat::courseBits)
        / Layer3dFormat::courseScale;
}

int ThreeDViewObject::aircraftScreenY() const
{
    return BitParser::extract(m_initHex, Layer3dFormat::screenYOffset, Layer3dFormat::screenYBits);
}

bool ThreeDViewObject::setAircraftState(double x, double y, double altitude, double course, int screenY)
{
    if (!qIsFinite(x) || !qIsFinite(y) || !qIsFinite(altitude) || !qIsFinite(course))
        return false;
    const QString previous = m_initHex;
    BitParser::inject(m_initHex, Layer3dFormat::xOffset, Layer3dFormat::coordinateBits, Layer3dFormat::positionToRaw(x));
    BitParser::inject(m_initHex, Layer3dFormat::yOffset, Layer3dFormat::coordinateBits, Layer3dFormat::positionToRaw(y));
    BitParser::inject(m_initHex, Layer3dFormat::heightOffset, Layer3dFormat::heightBits, Layer3dFormat::heightToRaw(altitude));
    BitParser::inject(m_initHex, Layer3dFormat::courseOffset, Layer3dFormat::courseBits, Layer3dFormat::courseToRaw(course));
    BitParser::inject(m_initHex, Layer3dFormat::screenYOffset, Layer3dFormat::screenYBits,
                      qBound(Layer3dFormat::minScreenY, screenY, Layer3dFormat::maxScreenY));
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
    const QRectF inner = rect.adjusted(2, 2, -2, -2);
    const double size = 24.0;
    const QPointF center = AircraftPositionMapping::center(inner, aircraftX(), aircraftY(), size);
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
    return QRectF(m_leftAnchored ? 0.0 : canvasW - side, 0.0, side, canvasH);
}
