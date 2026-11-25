// BankScaleObject — формат Python и приближённый предпросмотр фиксированной шкалы крена.
#include "BankScaleObject.h"
#include "BitParser.h"
#include "fpga/model/ScaleFormat.h"

#include <cmath>

namespace {
// Полный круг вмещает риски при любом крене; локальный размер рамки постоянен.
constexpr int extent = BankScaleFormat::fixedRadius + 14;
const QRectF localBounds(-extent, -extent, 2 * extent, 2 * extent);
}

BankScaleObject::BankScaleObject(QObject *parent) : BaseObject(parent) {}

QSharedPointer<BaseObject> BankScaleObject::clone() const
{
    auto copy = QSharedPointer<BankScaleObject>::create();
    copy->enabled = enabled;
    copy->centerX = centerX; copy->centerY = centerY; copy->bankAngle = bankAngle;
    copy->radius = radius; copy->tickColor = tickColor; copy->mark20Color = mark20Color;
    copy->colorValid = colorValid;
    copyEditorStateTo(*copy);
    return copy;
}

void BankScaleObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    auto field = [&](const char *name) {
        const auto info = schema.value(name);
        return BitParser::extract(hexInit, info.offset, info.size);
    };
    enabled = field("enable") != 0;
    centerX = field("center_x"); centerY = field("center_y");
    const auto angle = schema.value("bank_angle");
    bankAngle = BitParser::extractSigned(hexInit, angle.offset, angle.size) / 128.0;
    radius = int(field("radius"));
    tickColor = QColor::fromRgb(field("tick_color"));
    mark20Color = QColor::fromRgb(field("mark20_color"));
    colorValid = field("color_valid") != 0;
}

bool BankScaleObject::hasValidParameters() const
{
    return std::isfinite(centerX) && centerX >= 0 && centerX <= 4095
        && std::isfinite(centerY) && centerY >= 0 && centerY <= 4095
        && std::isfinite(bankAngle) && bankAngle >= -180 && bankAngle <= 180
        && radius >= 0 && radius <= 4095
        && tickColor.isValid() && tickColor.alpha() == 255
        && mark20Color.isValid() && mark20Color.alpha() == 255;
}

QMap<QString, quint32> BankScaleObject::serializeParams() const
{
    return {{"enable", enabled ? 1u : 0u},
        {"center_x", ScaleFormat::coordinate(centerX)}, {"center_y", ScaleFormat::coordinate(centerY)},
        {"bank_angle", ScaleFormat::angle(bankAngle)}, {"radius", quint32(radius)},
        {"tick_color", tickColor.rgb() & 0xffffffu}, {"mark20_color", mark20Color.rgb() & 0xffffffu},
        {"color_valid", colorValid ? 1u : 0u}};
}

QList<QPair<QString, QString>> BankScaleObject::getProperties() const
{
    return {{"Включен", enabled ? "да" : "нет"},
        {"Центр X", QString::number(centerX, 'g', 12)}, {"Центр Y", QString::number(centerY, 'g', 12)},
        {"Крен (°)", QString::number(bankAngle, 'g', 12)},
        {"Радиус (фикс.)", QString::number(BankScaleFormat::fixedRadius)},
        {"Цвет рисок", tickColor.name()}, {"Цвет отметок 20°", mark20Color.name()},
        {"Применять цвета", colorValid ? "да" : "нет"}};
}

bool BankScaleObject::setObjectProperty(const QString &name, const QString &value)
{
    const auto text = value.trimmed().toLower();
    if (name == "Включен" || name == "Применять цвета") {
        bool flag;
        if (text == "да" || text == "yes" || text == "true" || text == "1") flag = true;
        else if (text == "нет" || text == "no" || text == "false" || text == "0") flag = false;
        else return false;
        (name == "Включен" ? enabled : colorValid) = flag;
    } else if (name == "Цвет рисок" || name == "Цвет отметок 20°") {
        const QColor color(value.trimmed());
        if (!color.isValid() || color.alpha() != 255) {
            setValidationMessage(tr("Цвет должен быть непрозрачным RGB888.")); return false;
        }
        (name == "Цвет рисок" ? tickColor : mark20Color) = color;
    } else {
        bool ok = false;
        const double number = value.toDouble(&ok);
        if (!ok || !std::isfinite(number)) return false;
        if (name == "Центр X" || name == "Центр Y") {
            if (number < 0 || number > 4095) {
                setValidationMessage(tr("Координата центра должна быть в диапазоне 0…4095.")); return false;
            }
            (name == "Центр X" ? centerX : centerY) = ScaleFormat::roundEven(number);
        } else if (name == "Крен (°)") {
            if (number < -180 || number > 180) {
                setValidationMessage(tr("Крен должен быть в диапазоне −180…+180°.")); return false;
            }
            setRotation(number); return true;
        } else return false;
    }
    emit changed();
    return true;
}

QTransform BankScaleObject::drawingTransform() const
{
    QTransform transform;
    transform.translate(ScaleFormat::coordinate(centerX), ScaleFormat::coordinate(centerY));
    transform.rotate(ScaleFormat::quantizedAngle(bankAngle));
    return transform;
}

void BankScaleObject::draw(QPainter &painter)
{
    if (!enabled || !hasValidParameters()) return;
    // Python задаёт упаковку, но не рисунок RTL. Пока используем общую схему рисок
    // для редактора/симулятора; radius сохраняется в init и не масштабирует рисунок.
    const QColor tick = colorValid ? tickColor : QColor("#D3D3D3");
    const QColor mark20 = colorValid ? mark20Color : QColor("#9C4868");
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setTransform(drawingTransform(), true);
    for (int angle : {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60}) {
        painter.save();
        painter.rotate(angle);
        const int length = angle % 30 == 0 ? 14 : 8;
        const QLineF line(0, -BankScaleFormat::fixedRadius, 0, -BankScaleFormat::fixedRadius + length);
        painter.setPen(QPen(Qt::black, 5, Qt::SolidLine, Qt::SquareCap));
        painter.drawLine(line);
        painter.setPen(QPen(std::abs(angle) == 20 ? mark20 : tick, 2, Qt::SolidLine, Qt::SquareCap));
        painter.drawLine(line);
        painter.restore();
    }
    // Неподвижный указатель сравнивается с повёрнутыми рисками.
    painter.rotate(-ScaleFormat::quantizedAngle(bankAngle));
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(tick);
    painter.drawPolygon(QPolygonF({QPointF(0, -BankScaleFormat::fixedRadius - 2),
        QPointF(-5, -BankScaleFormat::fixedRadius - 11), QPointF(5, -BankScaleFormat::fixedRadius - 11)}));
    painter.restore();
}

QPolygonF BankScaleObject::selectionPolygon() const
{
    if (!hasValidParameters()) return {};
    return drawingTransform().map(QPolygonF(localBounds));
}

QRectF BankScaleObject::getBoundingRect() const { return selectionPolygon().boundingRect(); }

bool BankScaleObject::contains(const QPointF &point) const
{
    return hasValidParameters() && localBounds.contains(drawingTransform().inverted().map(point));
}

QTransform BankScaleObject::rotationFrame() const
{
    auto transform = drawingTransform();
    transform.translate(localBounds.x(), localBounds.y());
    transform.scale(localBounds.width(), localBounds.height());
    return transform;
}

QPointF BankScaleObject::rotationCenter() const
{
    return editorRotationPivot() ? rotationFrame().map(*editorRotationPivot()) : QPointF(centerX, centerY);
}

void BankScaleObject::moveBy(double dx, double dy)
{
    if (!std::isfinite(dx) || !std::isfinite(dy)) return;
    centerX = qBound(0.0, centerX + dx, 4095.0);
    centerY = qBound(0.0, centerY + dy, 4095.0);
    emit changed();
}

void BankScaleObject::setRotation(double angle)
{
    if (!std::isfinite(angle)) return;
    const double oldAngle = bankAngle;
    const QPointF pivot = rotationCenter();
    angle = std::fmod(angle, 360.0);
    if (angle > 180) angle -= 360;
    if (angle < -180) angle += 360;
    bankAngle = ScaleFormat::quantizedAngle(angle);
    if (editorRotationPivot()) {
        QTransform delta; delta.rotate(bankAngle - oldAngle);
        const QPointF center = pivot + delta.map(QPointF(centerX, centerY) - pivot);
        centerX = qBound(0.0, center.x(), 4095.0); centerY = qBound(0.0, center.y(), 4095.0);
    }
    emit changed();
}
