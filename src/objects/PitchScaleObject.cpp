// PitchScaleObject — формат Python и первое приближение рисунка по кадрам демонстрации.
#include "PitchScaleObject.h"
#include "BitParser.h"
#include "fpga/model/PitchScaleFormat.h"

#include <array>
#include <QCache>
#include <limits>
#include <cmath>

namespace {
constexpr int halfHeight = PitchScaleFormat::previewHeight / 2;
constexpr int halfWidth = PitchScaleFormat::previewWidth / 2;
const QRect localBounds(-halfWidth, -halfHeight, PitchScaleFormat::previewWidth, PitchScaleFormat::previewHeight);
constexpr int gap = 35;
constexpr int textX = 115;
constexpr int outline = 2;
constexpr int digitScale = 2;

// Рисунок не задаётся UART: фиксируем размеры для 1920×1080; цвет, шрифт и масштаб приблизительные.
// 44 угла восстановлены по кадрам: 22 положительных, затем 22 отрицательных.
const std::array<double, 22> positiveAngles = {
    2.5, 5, 7.5, 10, 12.5, 15, 17.5, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90
};
// Небольшой встроенный шрифт предпросмотра: не создаёт font/data или банк памяти ПЛИС.
const quint8 digits[10][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}
};

// Tick — положение, приоритет штриха и подпись одной аппаратной риски.
struct Tick {
    double position;
    int rank;
    int end;
    QString label;
};

bool glyphPixel(const QString &text, int x, int y)
{
    if (x < 0 || y < 0 || y >= 7 * digitScale) return false;
    const int column = x / digitScale;
    const int character = column / 6;
    if (character >= text.size() || column % 6 >= 5) return false;
    const int digit = text[character].digitValue();
    return digit >= 0 && (digits[digit][y / digitScale] & (1 << (4 - column % 6)));
}

bool textPixel(const Tick &tick, int u, int v)
{
    if (tick.label.isEmpty()) return false;
    const int width = (tick.label.size() * 6 - 1) * digitScale;
    const int y = v - PitchScaleFormat::roundEven(tick.position) + 7;
    return glyphPixel(tick.label, u - textX, y)
        || glyphPixel(tick.label, u + textX + width, y);
}
}

PitchScaleObject::PitchScaleObject(QObject *parent) : BaseObject(parent) {}

QSharedPointer<BaseObject> PitchScaleObject::clone() const
{
    auto copy = QSharedPointer<PitchScaleObject>::create();
    copy->enabled = enabled; copy->scaleMode = scaleMode;
    copy->centerX = centerX; copy->centerY = centerY;
    copy->pitchAngle = pitchAngle; copy->rotationAngle = rotationAngle;
    copyEditorStateTo(*copy);
    return copy;
}

void PitchScaleObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    auto field = [&](const char *name, bool signedValue = false) {
        const auto info = schema.value(name);
        return signedValue ? BitParser::extractSigned(hexInit, info.offset, info.size)
                           : int(BitParser::extract(hexInit, info.offset, info.size));
    };
    enabled = field("enable") != 0;
    scaleMode = field("scale_mode");
    centerX = field("center_x"); centerY = field("center_y");
    pitchAngle = field("pitch_angle", true) / 128.0;
    rotationAngle = field("rotation_angle", true) / 128.0;
}

bool PitchScaleObject::hasValidParameters() const
{
    return (scaleMode == 0 || scaleMode == 1)
        && std::isfinite(centerX) && centerX >= 0 && centerX <= 4095
        && std::isfinite(centerY) && centerY >= 0 && centerY <= 4095
        && std::isfinite(pitchAngle) && pitchAngle >= -90 && pitchAngle <= 90
        && std::isfinite(rotationAngle) && rotationAngle >= -180 && rotationAngle <= 180;
}

QMap<QString, quint32> PitchScaleObject::serializeParams() const
{
    return {{"enable", enabled ? 1u : 0u}, {"scale_mode", quint32(scaleMode)},
        {"center_x", PitchScaleFormat::coordinate(centerX)}, {"center_y", PitchScaleFormat::coordinate(centerY)},
        {"pitch_angle", PitchScaleFormat::angle(pitchAngle)}, {"rotation_angle", PitchScaleFormat::angle(rotationAngle)}};
}

QList<QPair<QString, QString>> PitchScaleObject::getProperties() const
{
    return {{"Включен", enabled ? "да" : "нет"},
        {"Режим шкалы", scaleMode == 0 ? "Линейная" : "Логарифмическая"},
        {"Центр X", QString::number(centerX, 'g', 12)}, {"Центр Y", QString::number(centerY, 'g', 12)},
        {"Тангаж (°)", QString::number(pitchAngle, 'g', 12)}, {"Угол (°)", QString::number(rotationAngle, 'g', 12)}};
}

bool PitchScaleObject::setObjectProperty(const QString &name, const QString &value)
{
    const auto text = value.trimmed().toLower();
    if (name == "Включен") {
        if (text == "да" || text == "yes" || text == "true" || text == "1") enabled = true;
        else if (text == "нет" || text == "no" || text == "false" || text == "0") enabled = false;
        else return false;
    } else if (name == "Режим шкалы") {
        if (text == "линейная" || text == "0") scaleMode = 0;
        else if (text == "логарифмическая" || text == "1") scaleMode = 1;
        else return false;
    } else {
        bool ok = false;
        const double number = value.toDouble(&ok);
        if (!ok || !std::isfinite(number)) return false;
        if (name == "Центр X" || name == "Центр Y") {
            if (number < 0 || number > 4095) {
                setValidationMessage(tr("Координата центра должна быть в диапазоне 0…4095.")); return false;
            }
            (name == "Центр X" ? centerX : centerY) = PitchScaleFormat::roundEven(number);
        } else if (name == "Тангаж (°)") {
            if (number < -90 || number > 90) {
                setValidationMessage(tr("Тангаж должен быть в диапазоне −90…+90°.")); return false;
            }
            pitchAngle = PitchScaleFormat::quantizedAngle(number);
        } else if (name == "Угол (°)") {
            if (number < -180 || number > 180) {
                setValidationMessage(tr("Угол поворота должен быть в диапазоне −180…+180°.")); return false;
            }
            setRotation(number); return true;
        } else return false;
    }
    emit changed();
    return true;
}

double PitchScaleObject::tickOffset(double deltaDegrees, int mode)
{
    // S на слайде не определён; калибруем логарифм по приведённому примеру: 17° -> 440.4 px.
    static const double logarithmCoefficient = 440.4 / std::log1p(17.0 / 5.0);
    // Линейный закон отсутствует в Python и на слайдах. В первом приближении
    // оба режима имеют одинаковое отклонение при |delta| = 20°.
    static const double linearCoefficient = logarithmCoefficient * std::log(5.0) / 20.0;
    if (mode == 0) return -deltaDegrees * linearCoefficient;
    return -std::copysign(logarithmCoefficient * std::log1p(std::abs(deltaDegrees) / 5.0), deltaDegrees);
}

void PitchScaleObject::updatePreview() const
{
    const int pitch = PitchScaleFormat::roundEven(pitchAngle * 128.0);
    if (!m_preview.isNull() && m_cachedPitch == pitch && m_cachedMode == scaleMode) return;
    m_cachedPitch = pitch; m_cachedMode = scaleMode;
    static QCache<int, QImage> cache(16);
    const int key = (pitch + 90 * 128) * 2 + scaleMode;
    if (const auto *preview = cache.object(key)) {
        m_preview = *preview; return;
    }
    std::array<Tick, 44> ticks;
    for (int i = 0; i < int(ticks.size()); ++i) {
        const double magnitude = positiveAngles[i % 22];
        const double angle = i < 22 ? magnitude : -magnitude;
        const int rank = std::fmod(magnitude, 10.0) == 0 ? 2 : (std::fmod(magnitude, 5.0) == 0 ? 1 : 0);
        ticks[i] = {tickOffset(angle - pitch / 128.0, scaleMode), rank,
            rank == 2 ? 105 : (rank == 1 ? 75 : 51), rank == 2 ? QString::number(int(magnitude)) : QString()};
    }
    m_preview = QImage(localBounds.size(), QImage::Format_ARGB32_Premultiplied);
    m_preview.fill(Qt::transparent);
    const QRgb foreground = QColor("#a3bf55").rgba();
    const QRgb black = qRgb(0, 0, 0);
    for (int v = -halfHeight; v < halfHeight; ++v) {
        int bar = -1, text = -1;
        double barDistance = std::numeric_limits<double>::max(), textDistance = barDistance;
        for (int i = 0; i < int(ticks.size()); ++i) {
            const auto &tick = ticks[i];
            const double distance = std::abs(v - tick.position);
            // Таблица строки выбирает штрих и текст независимо. Приоритет: длинная риска, затем ближайшая.
            if (distance <= 1 + outline && (bar < 0 || tick.rank > ticks[bar].rank
                || (tick.rank == ticks[bar].rank && distance < barDistance))) { bar = i; barDistance = distance; }
            if (!tick.label.isEmpty() && distance <= 7 + outline && distance < textDistance) { text = i; textDistance = distance; }
        }
        if (bar < 0 && text < 0) continue;
        auto *row = reinterpret_cast<QRgb *>(m_preview.scanLine(v + halfHeight));
        for (int u = -halfWidth; u < halfWidth; ++u) {
            bool color = false, border = false;
            if (bar >= 0) {
                const int x = std::abs(u);
                color = barDistance <= 1 && x >= gap && x <= ticks[bar].end;
                border = x >= gap - outline && x <= ticks[bar].end + outline;
            }
            if (text >= 0 && std::abs(u) >= textX - outline) {
                color |= textPixel(ticks[text], u, v);
                for (int dy = -outline; dy <= outline && !border; ++dy)
                    for (int dx = -outline; dx <= outline && !border; ++dx)
                        border = textPixel(ticks[text], u + dx, v + dy);
            }
            if (color || border) {
                row[u + halfWidth] = color ? foreground : black;
            }
        }
    }
    cache.insert(key, new QImage(m_preview));
}

QTransform PitchScaleObject::drawingTransform() const
{
    QTransform transform;
    transform.translate(PitchScaleFormat::coordinate(centerX), PitchScaleFormat::coordinate(centerY));
    transform.rotate(PitchScaleFormat::quantizedAngle(rotationAngle));
    return transform;
}

void PitchScaleObject::draw(QPainter &painter)
{
    if (!enabled || !hasValidParameters()) return;
    updatePreview();
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.setTransform(drawingTransform(), true);
    painter.drawImage(QPoint(-halfWidth, -halfHeight), m_preview);
    painter.restore();
}

QPolygonF PitchScaleObject::selectionPolygon() const
{
    if (!hasValidParameters()) return {};
    return drawingTransform().map(QPolygonF(QRectF(localBounds)));
}

QRectF PitchScaleObject::getBoundingRect() const { return selectionPolygon().boundingRect(); }

bool PitchScaleObject::contains(const QPointF &point) const
{
    if (!hasValidParameters()) return false;
    return QRectF(localBounds).contains(drawingTransform().inverted().map(point));
}

QTransform PitchScaleObject::rotationFrame() const
{
    QTransform transform = drawingTransform();
    transform.translate(localBounds.x(), localBounds.y());
    transform.scale(localBounds.width(), localBounds.height());
    return transform;
}

QPointF PitchScaleObject::rotationCenter() const
{
    return editorRotationPivot() ? rotationFrame().map(*editorRotationPivot()) : QPointF(centerX, centerY);
}

void PitchScaleObject::moveBy(double dx, double dy)
{
    if (!std::isfinite(dx) || !std::isfinite(dy)) return;
    centerX = qBound(0.0, centerX + dx, 4095.0);
    centerY = qBound(0.0, centerY + dy, 4095.0);
    emit changed();
}

void PitchScaleObject::setRotation(double angle)
{
    if (!std::isfinite(angle)) return;
    const double oldAngle = rotationAngle;
    const QPointF pivot = rotationCenter();
    angle = std::fmod(angle, 360.0);
    if (angle > 180) angle -= 360;
    if (angle < -180) angle += 360;
    rotationAngle = PitchScaleFormat::quantizedAngle(angle);
    if (editorRotationPivot()) {
        QTransform delta; delta.rotate(rotationAngle - oldAngle);
        const QPointF center = pivot + delta.map(QPointF(centerX, centerY) - pivot);
        centerX = qBound(0.0, center.x(), 4095.0); centerY = qBound(0.0, center.y(), 4095.0);
    }
    emit changed();
}
