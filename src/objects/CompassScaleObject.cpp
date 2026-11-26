// CompassScaleObject — формат PDF и общий приближённый предпросмотр редактора/симулятора.
#include "CompassScaleObject.h"

#include "BitParser.h"
#include "ProjectManager.h"
#include "ScaleGlyphs.h"
#include "fpga/model/CompassScaleFormat.h"

namespace {
constexpr int majorLength = 24;
constexpr int minorLength = 12;
constexpr int labelGap = 8;
constexpr int labelCell = 3;
constexpr int heightAbove = majorLength + labelGap + CompassScaleFormat::textHeight;
}

CompassScaleObject::CompassScaleObject(QObject *parent) : BaseObject(parent) {}

QSharedPointer<BaseObject> CompassScaleObject::clone() const
{
    auto copy = QSharedPointer<CompassScaleObject>::create();
    copy->positionY = positionY; copy->courseAngle = courseAngle; copy->color = color;
    copyEditorStateTo(*copy);
    return copy;
}

void CompassScaleObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    const auto y = schema.value("position_y");
    const auto course = schema.value("course_angle");
    const auto rgb = schema.value("color");
    positionY = BitParser::extract(hexInit, y.offset, y.size);
    courseAngle = BitParser::extractSigned(hexInit, course.offset, course.size) / 128.0;
    color = QColor::fromRgb(BitParser::extract(hexInit, rgb.offset, rgb.size));
}

bool CompassScaleObject::hasValidParameters() const
{
    return std::isfinite(positionY) && positionY >= 0 && positionY <= 4095
        && std::isfinite(courseAngle) && courseAngle >= CompassScaleFormat::minCourse
        && courseAngle <= CompassScaleFormat::maxCourse
        && color.isValid() && color.alpha() == 255;
}

QMap<QString, quint32> CompassScaleObject::serializeParams() const
{
    return {{"position_y", ScaleFormat::coordinate(positionY)},
        {"course_angle", CompassScaleFormat::course(courseAngle)}, {"color", color.rgb() & 0xffffffu}};
}

QList<QPair<QString, QString>> CompassScaleObject::getProperties() const
{
    return {{"Координата Y", QString::number(positionY, 'g', 12)},
        {"Курс (°)", QString::number(courseAngle, 'g', 12)}, {"Цвет шкалы", color.name()}};
}

bool CompassScaleObject::setObjectProperty(const QString &name, const QString &value)
{
    if (name == "Цвет шкалы") {
        const QColor rgb(value.trimmed());
        if (!rgb.isValid() || rgb.alpha() != 255) {
            setValidationMessage(tr("Цвет должен быть непрозрачным RGB888.")); return false;
        }
        color = rgb;
    } else {
        bool ok = false;
        const double number = value.toDouble(&ok);
        if (!ok || !std::isfinite(number)) return false;
        if (name == "Координата Y") {
            if (number < 0 || number > 4095) {
                setValidationMessage(tr("Координата Y должна быть в диапазоне 0…4095.")); return false;
            }
            positionY = ScaleFormat::roundEven(number);
        } else if (name == "Курс (°)") {
            if (number < CompassScaleFormat::minCourse || number > CompassScaleFormat::maxCourse) {
                setValidationMessage(tr("Курс должен помещаться в signed Q7 из 17 бит: −512…511,9921875°.")); return false;
            }
            courseAngle = ScaleFormat::quantizedAngle(number);
        } else return false;
    }
    emit changed();
    return true;
}

int CompassScaleObject::canvasWidth() const
{
    const int width = ProjectManager::instance()->getCanvasWidth();
    return width > 0 ? width : 1920;
}

QRectF CompassScaleObject::getBoundingRect() const
{
    return QRectF(0, double(ScaleFormat::coordinate(positionY)) - heightAbove, canvasWidth(), heightAbove + 2);
}

void CompassScaleObject::draw(QPainter &painter)
{
    drawForWidth(painter, canvasWidth());
}

void CompassScaleObject::drawForWidth(QPainter &painter, int width) const
{
    if (!hasValidParameters() || width <= 0) return;
    // В PDF нет font5x7_row и формул MAJ_LEN/MIN_LEN/LBL_OFF. Эти размеры — предпросмотр.
    const double pixelsPerDegree = width / (4.0 * CompassScaleFormat::fixedStepQ4 / 16.0);
    const double heading = ScaleFormat::quantizedAngle(courseAngle);
    const double halfSpan = width / (2.0 * pixelsPerDegree);
    const double y = ScaleFormat::coordinate(positionY);
    const int firstTick = int(std::floor((heading - halfSpan) / 5.0)) - 1;
    const int lastTick = int(std::ceil((heading + halfSpan) / 5.0)) + 1;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setClipRect(QRectF(0, y - heightAbove, width, heightAbove + 2), Qt::IntersectClip);
    painter.setPen(QPen(color, CompassScaleFormat::stroke));
    painter.drawLine(QPointF(0, y), QPointF(width, y));
    for (int tick = firstTick; tick <= lastTick; ++tick) {
        const double x = width / 2.0 + (tick * 5.0 - heading) * pixelsPerDegree;
        const bool major = tick % 2 == 0;
        painter.drawLine(QPointF(x, y), QPointF(x, y - (major ? majorLength : minorLength)));
        if (major) {
            const int degrees = ((tick * 5) % 360 + 360) % 360;
            const QString text = QString::number(degrees);
            const int glyphWidth = ScaleGlyphs::textWidth(text, labelCell);
            const int textWidth = qMin(CompassScaleFormat::textWidth, glyphWidth);
            painter.save();
            painter.translate(x - textWidth / 2.0, y - heightAbove);
            painter.scale(double(textWidth) / glyphWidth, 1);
            ScaleGlyphs::drawNumber(painter, text, QPointF(), labelCell, color);
            painter.restore();
        }
    }
    painter.restore();
}

void CompassScaleObject::moveBy(double dx, double dy)
{
    Q_UNUSED(dx);
    if (!std::isfinite(dy)) return;
    positionY = qBound(0.0, positionY + dy, 4095.0);
    emit changed();
}
