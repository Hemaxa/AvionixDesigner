// AircraftPositionPanel - панель позиционирования самолета в пространстве

#include "AircraftPositionPanel.h"
#include "AircraftPositionMapping.h"
#include "ProjectManager.h"
#include "ThreeDViewObject.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QFrame>
#include <QFontMetricsF>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolButton>
#include <QTransform>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>

namespace {
constexpr int kMinPosition = AircraftPositionMapping::minPosition;
constexpr int kMaxPosition = AircraftPositionMapping::maxPosition;
constexpr double kMinAltitude = 0.0;
constexpr double kMaxAltitude = Layer3dFormat::maxHeight;
constexpr int kDefaultAutopilotIntervalMs = 16;
constexpr double kAutopilotSpeedKmh = 1400.0;
constexpr double kMinimumPanelScale = 0.60;
constexpr double kMaximumPanelScale = 1.0;
constexpr double kCourseStep = 0.05;
constexpr double kFastCourseStep = 1.0;
constexpr double kFastAltitudeStep = 10.0;
constexpr int kFastScreenYStep = 10;

const QString kSliderThumbIcon =
    QStringLiteral(":/icons/icons/aircraft-position/slider-thumb.svg");
const QString kScreenIcon =
    QStringLiteral(":/icons/icons/aircraft-position/screen.svg");
const QString kAircraftIcon =
    QStringLiteral(":/icons/icons/aircraft-position/aircraft.svg");
const QString kAltitudeTrackIcon =
    QStringLiteral(":/icons/icons/aircraft-position/altitude-track.svg");
const QString kCourseBaseIcon =
    QStringLiteral(":/icons/icons/aircraft-position/course-base.svg");
const QString kCoursePointerIcon =
    QStringLiteral(":/icons/icons/aircraft-position/course-pointer.svg");
const QString kAutopilotOffIcon =
    QStringLiteral(":/icons/icons/aircraft-position/autopilot-button-off.svg");
const QString kAutopilotOnIcon =
    QStringLiteral(":/icons/icons/aircraft-position/autopilot-button-on.svg");

double normalizedCourse(double value) { return Layer3dFormat::normalizedCourse(value); }

QRectF fittedRect(const QRectF &bounds, const QSizeF &aspect) {
  if (bounds.isEmpty() || aspect.isEmpty())
    return bounds;

  const double targetRatio = aspect.width() / aspect.height();
  double width = bounds.width();
  double height = width / targetRatio;
  if (height > bounds.height()) {
    height = bounds.height();
    width = height * targetRatio;
  }

  return QRectF(bounds.center().x() - width * 0.5,
                bounds.center().y() - height * 0.5, width, height);
}

void drawSvg(QPainter *painter, QSvgRenderer &renderer, const QRectF &target) {
  if (renderer.isValid())
    renderer.render(painter, target);
}

// Числовое поле без лишних нулей; Shift увеличивает шаг стрелок и колеса.
class AircraftValueSpin : public QDoubleSpinBox {
public:
  explicit AircraftValueSpin(QWidget *parent) : QDoubleSpinBox(parent) {}
  double fastStep = 1.0;
protected:
  void stepBy(int steps) override {
    const double step = QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)
        ? fastStep : singleStep();
    const double value = this->value() + steps * step;
    if (wrapping()) setValue(Layer3dFormat::normalizedCourse(value));
    else setValue(value);
  }
  QString textFromValue(double value) const override {
    QLocale displayLocale = locale();
    displayLocale.setNumberOptions(displayLocale.numberOptions() | QLocale::OmitGroupSeparator);
    QString text = displayLocale.toString(value, 'f', decimals());
    while (text.endsWith(QLatin1Char('0')) && text.contains(locale().decimalPoint())) text.chop(1);
    if (text.endsWith(locale().decimalPoint())) text.chop(locale().decimalPoint().size());
    return text;
  }
};

// Беззнаковое поле yscreen; ускорение действует и при вращении колеса мыши.
class AircraftScreenYSpin : public QSpinBox {
public:
  using QSpinBox::QSpinBox;
protected:
  void stepBy(int steps) override {
    setValue(value() + steps * (QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)
        ? kFastScreenYStep : 1));
  }
};

QLabel *createCaption(const QString &text, QWidget *parent) {
  auto *label = new QLabel(text, parent);
  label->setObjectName(QStringLiteral("AircraftPositionCaption"));
  label->setAlignment(Qt::AlignCenter);
  return label;
}
} // namespace

// Квадратный экран позиционирования; координаты берутся из общей модели 3D-вида.
class AircraftPositionPanel::ScreenWidget : public QWidget {
public:
  explicit ScreenWidget(QWidget *parent = nullptr)
      : QWidget(parent), m_screenRenderer(kScreenIcon),
        m_aircraftRenderer(kAircraftIcon) {
    setObjectName(QStringLiteral("AircraftPositionScreen"));
    m_screenRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
    m_aircraftRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
    setMinimumSize(240, 240);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  }

  void setState(double x, double y, double altitude, double course) {
    m_x = x;
    m_y = y;
    m_altitude = altitude;
    m_course = course;
    setToolTip(AircraftPositionMapping::coordinateLabel(x, y));
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    // Фон совпадает с подложкой SVG; квадрат задается геометрией самого виджета.
    // Круги и оси не зависят от пропорций окна панели.
    painter.fillRect(rect(), QColor(QStringLiteral("#22303c")));

    const QRectF screenRect =
        fittedRect(rect(), QSizeF(1.0, 1.0));
    drawSvg(&painter, m_screenRenderer, screenRect);

    const double aircraftHeight =
        screenRect.height() *
        (0.16 + 0.04 * qBound(kMinAltitude, m_altitude, kMaxAltitude)
                           / static_cast<double>(kMaxAltitude));
    const double aircraftWidth = aircraftHeight * 0.48;
    const QRectF aircraftRect(-aircraftWidth * 0.5, -aircraftHeight * 0.54,
                              aircraftWidth, aircraftHeight);
    // Размещаем повернутый самолет с учетом его габаритов у границ карты.
    const QRectF rotatedBounds = QTransform().rotate(90.0 - m_course).mapRect(aircraftRect);
    const QPointF aircraftCenter = AircraftPositionMapping::center(
        screenRect, m_x, m_y, rotatedBounds.size()) - rotatedBounds.center();

    painter.save();
    painter.setClipRect(screenRect);
    painter.translate(aircraftCenter);
    painter.rotate(90.0 - m_course);
    drawSvg(&painter, m_aircraftRenderer, aircraftRect);
    painter.restore();

    painter.save();
    painter.setPen(QPen(QColor(0, 200, 240, 180), 1));
    painter.drawEllipse(aircraftCenter, 4, 4);
    painter.restore();
  }

private:
  QSvgRenderer m_screenRenderer;
  QSvgRenderer m_aircraftRenderer;
  double m_x = 0;
  double m_y = 0;
  double m_altitude = 0.0;
  double m_course = 0;
};

// Ползунок высоты с четвертями метра и ускоренным шагом при Shift.
class AircraftPositionPanel::AltitudeSliderWidget : public QWidget {
public:
  explicit AltitudeSliderWidget(QWidget *parent = nullptr)
      : QWidget(parent), m_trackRenderer(kAltitudeTrackIcon),
        m_thumbRenderer(kSliderThumbIcon) {
    setObjectName(QStringLiteral("AircraftAltitudeSlider"));
    m_trackRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
    m_thumbRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
    setMinimumSize(92, 170);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
  }

  void setValue(double value) {
    const double nextValue = qRound(std::clamp(value, kMinAltitude, kMaxAltitude)
                                    * Layer3dFormat::heightUnitsPerMeter) * Layer3dFormat::heightStep;
    if (m_value == nextValue)
      return;
    m_value = nextValue;
    update();
  }

  double value() const { return m_value; }
  void setControlScale(double scale) {
    setFixedSize(qRound(170 * scale), qRound(232 * scale));
    update();
  }
  std::function<void(double)> valueChanged;

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const double marginY = height() * 0.095;
    const double trackWidth = width() * 0.26;
    const QRectF trackRect(width() * 0.17, marginY, trackWidth,
                           height() - marginY * 2.0);
    const QRectF valueRange(trackRect.left(),
                            trackRect.top() + trackRect.height() * 0.12,
                            trackRect.width(), trackRect.height() * 0.76);
    drawSvg(&painter, m_trackRenderer, trackRect);

    const double tickX = trackRect.right() + width() * 0.10;
    painter.setPen(QPen(QColor(237, 247, 251, 220),
                        qMax(3, qRound(width() * 0.022)), Qt::SolidLine,
                        Qt::RoundCap));
    const int tickCount = 10;
    for (int i = 0; i <= tickCount; ++i) {
      const double t = static_cast<double>(i) / tickCount;
      const double y = valueRange.top() + t * valueRange.height();
      const double tickLength = (i % 5 == 0) ? width() * 0.12 : width() * 0.08;
      painter.drawLine(QPointF(tickX, y), QPointF(tickX + tickLength, y));
    }

    painter.setPen(QColor(237, 247, 251));
    QFont valueFont = painter.font();
    valueFont.setPixelSize(qMax(7, qRound(width() * 0.055)));
    valueFont.setWeight(QFont::DemiBold);
    painter.setFont(valueFont);
    const double labelX = tickX + width() * 0.15;
    painter.drawText(
        QRectF(labelX, valueRange.top() - 14, width() - labelX, 28),
        Qt::AlignLeft | Qt::AlignVCenter, QString::number(kMaxAltitude, 'f', 2));
    painter.drawText(
        QRectF(labelX, valueRange.bottom() - 14, width() - labelX, 28),
        Qt::AlignLeft | Qt::AlignVCenter, QString::number(kMinAltitude));

    const double ratio = (m_value - kMinAltitude) /
                         static_cast<double>(kMaxAltitude - kMinAltitude);
    const double thumbY = valueRange.bottom() - ratio * valueRange.height();
    painter.setPen(
        QPen(QColor(6, 17, 22, 150), 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(trackRect.center().x(), trackRect.top() + 25),
                     QPointF(trackRect.center().x(), trackRect.bottom() - 25));

    const double thumbWidth = trackRect.width() * 1.38;
    const QRectF thumb(trackRect.center().x() - thumbWidth * 0.5,
                       thumbY - thumbWidth * 0.36, thumbWidth,
                       thumbWidth * 0.72);
    drawSvg(&painter, m_thumbRenderer, thumb);
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton)
      setValueFromY(event->position().y());
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (event->buttons() & Qt::LeftButton)
      setValueFromY(event->position().y());
  }

  void wheelEvent(QWheelEvent *event) override {
    const int delta = event->angleDelta().y();
    if (delta == 0)
      return;
    const double step = event->modifiers() & Qt::ShiftModifier ? kFastAltitudeStep : Layer3dFormat::heightStep;
    updateValue(m_value + (delta > 0 ? step : -step));
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_PageUp) {
      updateValue(m_value + 100.0);
    } else if (event->key() == Qt::Key_PageDown) {
      updateValue(m_value - 100.0);
    } else {
      QWidget::keyPressEvent(event);
    }
  }

private:
  void setValueFromY(double y) {
    const double marginY = height() * 0.095;
    const double trackWidth = width() * 0.26;
    const QRectF trackRect(width() * 0.17, marginY, trackWidth,
                           height() - marginY * 2.0);
    const QRectF valueRange(trackRect.left(),
                            trackRect.top() + trackRect.height() * 0.12,
                            trackRect.width(), trackRect.height() * 0.76);
    const double ratio =
        std::clamp((valueRange.bottom() - y) / valueRange.height(), 0.0, 1.0);
    updateValue(kMinAltitude + ratio * (kMaxAltitude - kMinAltitude));
  }

  void updateValue(double value) {
    const double nextValue = qRound(std::clamp(value, kMinAltitude, kMaxAltitude)
                                    * Layer3dFormat::heightUnitsPerMeter) * Layer3dFormat::heightStep;
    if (nextValue == m_value)
      return;
    m_value = nextValue;
    update();
    if (valueChanged)
      valueChanged(m_value);
  }

  QSvgRenderer m_trackRenderer;
  QSvgRenderer m_thumbRenderer;
  double m_value = 0.0;
};

// Круговой регулятор курса: ноль направлен вдоль +X, 90° вдоль +Y.
class AircraftPositionPanel::CourseKnobWidget : public QWidget {
public:
  explicit CourseKnobWidget(QWidget *parent = nullptr)
      : QWidget(parent), m_baseRenderer(kCourseBaseIcon),
        m_pointerRenderer(kCoursePointerIcon) {
    setObjectName(QStringLiteral("AircraftCourseKnob"));
    m_baseRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
    m_pointerRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
    setMinimumSize(170, 146);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
  }

  void setCourse(double course) {
    const double nextCourse = normalizedCourse(course);
    if (m_course == nextCourse)
      return;
    m_course = nextCourse;
    update();
  }

  double course() const { return m_course; }
  void setControlScale(double scale) {
    setFixedSize(qRound(260 * scale), qRound(224 * scale));
    update();
  }
  std::function<void(double)> courseChanged;

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QPointF center = rect().center();
    const double baseSize = height() * 0.6;
    const double knobSize = height() * 0.46;
    const QRectF baseRect(center.x() - baseSize * 0.5,
                          center.y() - baseSize * 0.5, baseSize, baseSize);
    const QRectF knobRect(center.x() - knobSize * 0.5,
                          center.y() - knobSize * 0.5, knobSize, knobSize);
    drawSvg(&painter, m_baseRenderer, baseRect);

    painter.save();
    painter.translate(knobRect.center());
    painter.rotate(90.0 - m_course);
    const double pointerSize = knobRect.width() * 0.95;
    drawSvg(&painter, m_pointerRenderer,
            QRectF(-pointerSize * 0.5, -pointerSize * 0.5, pointerSize,
                   pointerSize));
    painter.restore();

    for (int degrees = 0; degrees < 360; degrees += 15) {
      const bool cardinal = degrees % 90 == 0;
      const bool major = degrees % 45 == 0;
      const double angle = qDegreesToRadians(static_cast<double>(degrees));
      const QPointF direction(std::sin(angle), -std::cos(angle));
      const double radius = baseRect.width() * 0.5;
      const double startRadius = radius + height() * 0.035;
      const double tickLength = cardinal ? height() * 0.026
                                : major  ? height() * 0.050
                                         : height() * 0.030;
      const double endRadius = startRadius + tickLength;
      painter.setPen(QPen(QColor(237, 247, 251),
                          major ? qMax(3, qRound(width() * 0.018))
                                : qMax(2, qRound(width() * 0.010)),
                          Qt::SolidLine, Qt::RoundCap));
      const QPointF p1 = baseRect.center() + direction * startRadius;
      const QPointF p2 = baseRect.center() + direction * endRadius;
      painter.drawLine(p1, p2);
    }

    painter.setPen(QColor(237, 247, 251));
    QFont cardinalFont = painter.font();
    cardinalFont.setPixelSize(qBound(9, qRound(height() * 0.05), 12));
    cardinalFont.setWeight(QFont::DemiBold);
    painter.setFont(cardinalFont);
    drawCardinalLabel(&painter, baseRect, QStringLiteral("С 90°"), 0);
    drawCardinalLabel(&painter, baseRect, QStringLiteral("В 0°"), 90);
    drawCardinalLabel(&painter, baseRect, QStringLiteral("Ю 270°"), 180);
    drawCardinalLabel(&painter, baseRect, QStringLiteral("З 180°"), 270);
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) updateFromPosition(event->position());
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (event->buttons() & Qt::LeftButton)
      updateFromPosition(event->position());
  }

  void wheelEvent(QWheelEvent *event) override {
    if (!event->angleDelta().y()) return;
    const int direction = event->angleDelta().y() > 0 ? 1 : -1;
    updateCourse(m_course +
                 direction * (event->modifiers() & Qt::ShiftModifier ? kFastCourseStep : kCourseStep));
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Left) {
      updateCourse(m_course - (event->modifiers() & Qt::ShiftModifier ? kFastCourseStep : kCourseStep));
    } else if (event->key() == Qt::Key_Right) {
      updateCourse(m_course + (event->modifiers() & Qt::ShiftModifier ? kFastCourseStep : kCourseStep));
    } else {
      QWidget::keyPressEvent(event);
    }
  }

private:
  void drawCardinalLabel(QPainter *painter, const QRectF &outer,
                         const QString &text, double degrees) {
    QRectF labelRect;
    const double labelHeight = height() * 0.105;
    if (degrees == 0) {
      labelRect = QRectF(width() * 0.39, 0, width() * 0.22, labelHeight);
    } else if (degrees == 90) {
      labelRect = QRectF(width() * 0.83, outer.center().y() - labelHeight * 0.5,
                         width() * 0.17, labelHeight);
    } else if (degrees == 180) {
      labelRect = QRectF(width() * 0.37, height() - labelHeight, width() * 0.26,
                         labelHeight);
    } else {
      labelRect = QRectF(0, outer.center().y() - labelHeight * 0.5,
                         width() * 0.17, labelHeight);
    }
    painter->drawText(labelRect, Qt::AlignCenter, text);
  }

  void updateFromPosition(const QPointF &position) {
    const QPointF center = rect().center();
    const QPointF delta = position - center;
    updateCourse(qRound(normalizedCourse(qRadiansToDegrees(std::atan2(-delta.y(), delta.x()))) * 20.0) / 20.0);
  }

  void updateCourse(double course) {
    const double nextCourse = normalizedCourse(course);
    if (nextCourse == m_course)
      return;
    m_course = nextCourse;
    update();
    if (courseChanged)
      courseChanged(m_course);
  }

  QSvgRenderer m_baseRenderer;
  QSvgRenderer m_pointerRenderer;
  double m_course = 0;
};

AircraftPositionPanel::AircraftPositionPanel(QWidget *parent)
    : BasePanel(parent) {
  setPanelName(QStringLiteral("AircraftPositionPanel"));
  setMinimumSize(800, 360);
  setMaximumSize(1600, 900);
  setFocusPolicy(Qt::StrongFocus);

  m_screenWidget = new ScreenWidget(this);
  auto createFractionalSpin = [this](const QString &name, double min, double max, double step) {
    auto *spin = new AircraftValueSpin(this);
    spin->setObjectName(name);
    spin->setDecimals(6);
    spin->setRange(min, max);
    spin->setSingleStep(step);
    spin->setKeyboardTracking(false);
    spin->setGroupSeparatorShown(false);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setAlignment(Qt::AlignCenter);
    return spin;
  };
  m_xSpin = createFractionalSpin(QStringLiteral("AircraftXSpin"), kMinPosition, kMaxPosition, 1.0 / 64);
  m_ySpin = createFractionalSpin(QStringLiteral("AircraftYSpin"), kMinPosition, kMaxPosition, 1.0 / 64);
  m_xSpin->setPrefix(QStringLiteral("X: "));
  m_ySpin->setPrefix(QStringLiteral("Y: "));
  m_xSpin->setToolTip(QStringLiteral("X в точках карты: 1 точка = 32 м; шаг 1/64 точки"));
  m_ySpin->setToolTip(QStringLiteral("Y в точках карты: 1 точка = 32 м; увеличивается вверх"));
  auto *altitudeSpin = new AircraftValueSpin(this);
  altitudeSpin->fastStep = kFastAltitudeStep;
  m_altitudeSpin = altitudeSpin;
  m_altitudeSpin->setObjectName(QStringLiteral("AircraftAltitudeSpin"));
  m_altitudeSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  m_altitudeSpin->setAlignment(Qt::AlignCenter);
  m_altitudeSpin->setDecimals(2);
  m_altitudeSpin->setKeyboardTracking(false);
  m_altitudeSpin->setRange(kMinAltitude, kMaxAltitude);
  m_altitudeSpin->setSingleStep(Layer3dFormat::heightStep);
  m_altitudeSpin->setSuffix(QStringLiteral(" м"));
  m_altitudeSpin->setToolTip(QStringLiteral("Высота самолета с шагом 0,25 м"));
  m_courseSpin = createFractionalSpin(QStringLiteral("AircraftCourseSpin"), 0, 359.999313, kCourseStep);
  m_courseSpin->setSuffix(QStringLiteral("°"));
  m_courseSpin->setWrapping(true);
  m_courseSpin->setToolTip(QStringLiteral("Курс ПЛИС: 0° = +X, 90° = +Y"));
  m_screenYSpin = createValueSpin(QStringLiteral("AircraftScreenYSpin"),
      Layer3dFormat::minScreenY, Layer3dFormat::maxScreenY, Layer3dFormat::defaultScreenY);
  m_screenYSpin->setToolTip(QStringLiteral("Вертикальная координата экрана: 0…1023"));
  auto createScreenYButton = [this](const QString &direction, const QString &tooltip) {
    auto *button = new QToolButton(this);
    button->setObjectName(QStringLiteral("AircraftScreenYButton"));
    button->setAccessibleName(tooltip);
    button->setToolTip(tooltip);
    button->setAutoRaise(true);
    button->setAutoRepeat(true);
    button->setAutoRepeatDelay(350);
    button->setAutoRepeatInterval(90);
    const QString normal = QStringLiteral(":/icons/icons/aircraft-position/screen-y-%1.svg").arg(direction);
    const QString pressed = QStringLiteral(":/icons/icons/aircraft-position/screen-y-%1-pressed.svg").arg(direction);
    button->setIcon(QIcon(normal));
    connect(button, &QToolButton::pressed, this, [button, pressed]() { button->setIcon(QIcon(pressed)); });
    connect(button, &QToolButton::released, this, [button, normal]() { button->setIcon(QIcon(normal)); });
    return button;
  };
  m_screenYUpButton = createScreenYButton(QStringLiteral("up"), QStringLiteral("Увеличить тангаж (↑; Shift — шаг 10)"));
  m_screenYDownButton = createScreenYButton(QStringLiteral("down"), QStringLiteral("Уменьшить тангаж (↓; Shift — шаг 10)"));
  m_altitudeSlider = new AltitudeSliderWidget(this);
  m_altitudeSlider->setToolTip(QStringLiteral("Шаг 0,25 м: +/− или колесо мыши.\n"
                                           "Шаг 10 м: Shift + колесо; шаг 100 м: Page Up/Page Down."));
  m_courseKnob = new CourseKnobWidget(this);
  m_courseControlArea = new QWidget(this);
  m_altitudeControlArea = new QWidget(this);
  m_autopilotButton = new QToolButton(this);
  m_autopilotTimer = new QTimer(this);
  m_autopilotTimer->setInterval(kDefaultAutopilotIntervalMs);
  m_autopilotTimer->setTimerType(Qt::PreciseTimer);

  m_autopilotButton->setObjectName(QStringLiteral("AircraftAutopilotButton"));
  m_autopilotButton->setCheckable(true);
  m_autopilotButton->setAutoRaise(true);
  m_autopilotButton->setFixedSize(92, 54);
  m_autopilotButton->setIconSize(QSize(92, 54));
  m_autopilotButton->setToolTip(QStringLiteral("Автопилот: 1400 км/ч; 0° = +X, 90° = +Y"));

  auto *coordinateLayout = new QGridLayout();
  coordinateLayout->setContentsMargins(0, 0, 0, 0);
  coordinateLayout->setHorizontalSpacing(8);
  coordinateLayout->setVerticalSpacing(6);
  coordinateLayout->addWidget(m_xSpin, 0, 0, Qt::AlignCenter);
  coordinateLayout->addWidget(m_ySpin, 1, 0, Qt::AlignCenter);

  m_screenYControlArea = new QWidget(this);
  auto *screenYLayout = new QGridLayout(m_screenYControlArea);
  screenYLayout->setContentsMargins(0, 0, 0, 0);
  screenYLayout->setHorizontalSpacing(8);
  screenYLayout->setVerticalSpacing(6);
  screenYLayout->addWidget(m_screenYSpin, 0, 0, 2, 1, Qt::AlignCenter);
  screenYLayout->addWidget(m_screenYUpButton, 0, 1, Qt::AlignCenter);
  screenYLayout->addWidget(m_screenYDownButton, 1, 1, Qt::AlignCenter);

  auto *courseControlLayout = new QVBoxLayout(m_courseControlArea);
  courseControlLayout->setContentsMargins(0, 0, 0, 0);
  courseControlLayout->setSpacing(0);
  courseControlLayout->addStretch(1);
  courseControlLayout->addWidget(m_courseKnob, 0, Qt::AlignCenter);
  courseControlLayout->addStretch(1);
  m_courseControlArea->setFixedSize(260, 232);

  auto *altitudeControlLayout = new QVBoxLayout(m_altitudeControlArea);
  altitudeControlLayout->setContentsMargins(0, 0, 0, 0);
  altitudeControlLayout->setSpacing(0);
  altitudeControlLayout->addWidget(m_altitudeSlider, 0, Qt::AlignCenter);
  m_altitudeControlArea->setFixedSize(170, 232);

  auto *courseLayout = new QVBoxLayout();
  courseLayout->setContentsMargins(0, 0, 0, 0);
  courseLayout->setSpacing(6);
  courseLayout->addWidget(createCaption(QStringLiteral("Курс"), this), 0,
                          Qt::AlignCenter);
  courseLayout->addWidget(m_courseControlArea, 0, Qt::AlignCenter);
  courseLayout->addWidget(m_courseSpin, 0, Qt::AlignCenter);

  auto *altitudeLayout = new QVBoxLayout();
  altitudeLayout->setContentsMargins(0, 0, 0, 0);
  altitudeLayout->setSpacing(6);
  altitudeLayout->addWidget(createCaption(QStringLiteral("Высота"), this), 0,
                            Qt::AlignCenter);
  altitudeLayout->addWidget(m_altitudeControlArea, 0, Qt::AlignCenter);
  altitudeLayout->addWidget(m_altitudeSpin, 0, Qt::AlignCenter);

  m_controlsLayout = new QGridLayout();
  m_controlsLayout->setContentsMargins(8, 6, 8, 6);
  m_controlsLayout->setHorizontalSpacing(10);
  m_controlsLayout->setVerticalSpacing(32);
  m_controlsLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
  auto *topRow = new QGridLayout();
  topRow->setHorizontalSpacing(20);
  topRow->setVerticalSpacing(6);
  topRow->addWidget(createCaption(QStringLiteral("Координаты"), this), 0, 0);
  topRow->addWidget(createCaption(QStringLiteral("Тангаж"), this), 0, 1);
  topRow->addWidget(createCaption(QStringLiteral("Автопилот"), this), 0, 2);
  topRow->addLayout(coordinateLayout, 1, 0, Qt::AlignCenter);
  topRow->addWidget(m_screenYControlArea, 1, 1, Qt::AlignCenter);
  topRow->addWidget(m_autopilotButton, 1, 2, Qt::AlignCenter);
  auto *bottomRow = new QHBoxLayout();
  bottomRow->setSpacing(10);
  bottomRow->addLayout(courseLayout);
  bottomRow->addLayout(altitudeLayout);
  m_controlsLayout->addLayout(topRow, 0, 0, Qt::AlignHCenter | Qt::AlignTop);
  m_controlsLayout->addLayout(bottomRow, 1, 0, Qt::AlignHCenter | Qt::AlignTop);

  m_mainLayout = new QHBoxLayout(this);
  m_mainLayout->setContentsMargins(8, 8, 8, 8);
  m_mainLayout->setSpacing(12);
  m_mainLayout->addWidget(m_screenWidget, 0, Qt::AlignLeft | Qt::AlignVCenter);
  m_mainLayout->addLayout(m_controlsLayout, 1);

  m_unavailableOverlay = new QFrame(this);
  m_unavailableOverlay->setObjectName(QStringLiteral("AircraftPositionUnavailableOverlay"));
  auto *overlayLayout = new QVBoxLayout(m_unavailableOverlay);
  overlayLayout->setContentsMargins(28, 26, 28, 26);
  overlayLayout->setSpacing(8);
  overlayLayout->addStretch(1);
  auto *overlayTitle = new QLabel(QStringLiteral("Панель недоступна"), m_unavailableOverlay);
  overlayTitle->setObjectName(QStringLiteral("AircraftPositionUnavailableTitle"));
  overlayTitle->setAlignment(Qt::AlignCenter);
  auto *overlayText = new QLabel(QStringLiteral("Добавьте объект \"3D вид\" в библиотеке объектов, чтобы включить позиционирование самолета."), m_unavailableOverlay);
  overlayText->setObjectName(QStringLiteral("AircraftPositionUnavailableText"));
  overlayText->setAlignment(Qt::AlignCenter);
  overlayText->setWordWrap(true);
  overlayLayout->addWidget(overlayTitle);
  overlayLayout->addWidget(overlayText);
  overlayLayout->addStretch(1);
  m_unavailableOverlay->hide();

  connect(m_xSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double value) {
            if (!m_updatingControls)
              m_preciseX = value;
            syncScreen();
          });
  connect(m_ySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double value) {
            if (!m_updatingControls)
              m_preciseY = value;
            syncScreen();
          });
  connect(m_altitudeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double value) {
            const double snapped = Layer3dFormat::roundRaw(value * Layer3dFormat::heightUnitsPerMeter) * Layer3dFormat::heightStep;
            const QSignalBlocker blocker(m_altitudeSpin);
            m_altitudeSpin->setValue(snapped);
            m_altitudeSlider->setValue(snapped);
            syncScreen();
          });
  connect(m_courseSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double value) {
            m_courseKnob->setCourse(value);
            syncScreen();
          });
  connect(m_screenYSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [this]() { syncScreen(); });
  connect(m_screenYUpButton, &QToolButton::pressed, this, [this]() {
    adjustScreenY(1, QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier));
  });
  connect(m_screenYDownButton, &QToolButton::pressed, this, [this]() {
    adjustScreenY(-1, QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier));
  });
  connect(m_autopilotButton, &QToolButton::clicked, this,
          [this](bool checked) { setAutopilotEnabled(checked); });
  connect(m_autopilotTimer, &QTimer::timeout, this,
          &AircraftPositionPanel::advanceAutopilot);

  m_altitudeSlider->valueChanged = [this](double value) {
    QSignalBlocker blocker(m_altitudeSpin);
    m_altitudeSpin->setValue(value);
    syncScreen();
  };
  m_courseKnob->courseChanged = [this](double course) {
    QSignalBlocker blocker(m_courseSpin);
    m_courseSpin->setValue(course);
    syncScreen();
  };

  syncAutopilotButtons();
  connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, [this]() {
    m_syncingFromProject = true;
    setAutopilotEnabled(false);
    m_syncingFromProject = false;
    syncFromProject();
  });
  connect(ProjectManager::instance(), &ProjectManager::projectChanged,
          this, &AircraftPositionPanel::syncFromProject);
  syncFromProject();
  applyControlScale();
  updateAvailabilityOverlay();
  installEventFilter(this);
  for (auto *widget : findChildren<QWidget *>()) widget->installEventFilter(this);
  m_screenWidget->setFocus();
}

void AircraftPositionPanel::resizeEvent(QResizeEvent *event) {
  BasePanel::resizeEvent(event);
  applyControlScale();
  updateAvailabilityOverlay();
}

void AircraftPositionPanel::setThreeDViewAvailable(bool available) {
  if (m_threeDViewAvailable == available)
    return;

  m_threeDViewAvailable = available;
  if (!m_threeDViewAvailable)
    setAutopilotEnabled(false);
  updateAvailabilityOverlay();
}

void AircraftPositionPanel::applyControlScale() {
  const double widthScale = width() / 920.0;
  // Учитываем оба поля координат над нижним рядом курса и высоты.
  const double heightScale = height() / 500.0;
  const double rawScale = std::clamp(std::min(widthScale, heightScale),
                                     kMinimumPanelScale, kMaximumPanelScale);
  double scale = 0.88;
  if (rawScale < 0.74) {
    scale = 0.64;
  } else if (rawScale < 0.84) {
    scale = 0.76;
  } else if (rawScale < 0.96) {
    scale = 0.88;
  } else {
    scale = 1.0;
  }

  const double layoutScale =
      std::clamp(rawScale, kMinimumPanelScale, kMaximumPanelScale);

  if (m_mainLayout) {
    const int margin = qRound(8 * layoutScale);
    m_mainLayout->setContentsMargins(margin, margin, margin, margin);
    m_mainLayout->setSpacing(qRound(12 * layoutScale));
  }

  if (m_controlsLayout) {
    m_controlsLayout->setContentsMargins(
        qRound(8 * layoutScale), qRound(6 * layoutScale),
        qRound(8 * layoutScale), qRound(6 * layoutScale));
    m_controlsLayout->setHorizontalSpacing(qRound(10 * layoutScale));
    // При достаточной высоте увеличиваем расстояние между рядами управления.
    const double rowSpacing = 22.0 + std::clamp((height() - 420.0) / 18.0, 0.0, 1.0) * 10.0;
    m_controlsLayout->setVerticalSpacing(qRound(rowSpacing * layoutScale));
  }

  auto fitScreen = [this, layoutScale]() {
    const int margin = qRound(8 * layoutScale);
    const int side = qMax(240, qMin(height() - 2 * margin,
        width() - m_controlsLayout->sizeHint().width() - 2 * margin - m_mainLayout->spacing()));
    m_screenWidget->setFixedSize(side, side);
  };
  if (qFuzzyCompare(m_appliedControlScale, scale)) {
    fitScreen();
    return;
  }
  m_appliedControlScale = scale;

  const int compactFieldHeight = qRound(std::max(34.0, 34.0 * scale));
  const QSize coordinateFieldSize(qRound(std::max(178.0, 178.0 * scale)),
                                  compactFieldHeight);
  const QSize courseFieldSize(qRound(std::max(128.0, 128.0 * scale)),
                              compactFieldHeight);
  const QSize altitudeFieldSize(qRound(std::max(140.0, 148.0 * scale)),
                                compactFieldHeight);
  m_xSpin->setFixedSize(coordinateFieldSize);
  m_ySpin->setFixedSize(coordinateFieldSize);
  m_courseSpin->setFixedSize(courseFieldSize);
  m_screenYSpin->setFixedSize(92, compactFieldHeight);
  m_altitudeSpin->setFixedSize(altitudeFieldSize);

  m_courseKnob->setControlScale(scale);
  m_altitudeSlider->setControlScale(scale);
  m_courseControlArea->setFixedSize(qRound(260 * scale), qRound(232 * scale));
  m_altitudeControlArea->setFixedSize(qRound(170 * scale), qRound(232 * scale));

  const QSize autopilotSize(qRound(92 * scale), qRound(54 * scale));
  m_autopilotButton->setFixedSize(autopilotSize);
  m_autopilotButton->setIconSize(autopilotSize);
  const QSize screenYButtonSize(qRound(48 * scale), qRound(std::max(30.0, 40.0 * scale)));
  for (QToolButton *button : {m_screenYUpButton, m_screenYDownButton}) {
    button->setFixedSize(screenYButtonSize);
    button->setIconSize(screenYButtonSize);
  }
  m_screenYControlArea->setFixedSize(m_screenYSpin->width() + 8 + screenYButtonSize.width(),
                                   2 * screenYButtonSize.height() + 6);

  const int captionSize = qRound(15 * scale);
  const QList<QLabel *> labels =
      findChildren<QLabel *>(QStringLiteral("AircraftPositionCaption"));
  for (QLabel *label : labels) {
    QFont font = label->font();
    font.setPointSize(qMax(12, captionSize));
    font.setWeight(QFont::Bold);
    label->setFont(font);
    label->setFixedHeight(qRound(24 * scale));
  }
  fitScreen();
}

QSpinBox *AircraftPositionPanel::createValueSpin(const QString &objectName,
                                                 int min, int max, int value,
                                                 const QString &suffix) {
  auto *spin = new AircraftScreenYSpin(this);
  spin->setObjectName(objectName);
  spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spin->setAlignment(Qt::AlignCenter);
  spin->setRange(min, max);
  spin->setValue(value);
  spin->setSuffix(suffix);
  spin->setFixedSize(84, 32);
  spin->setToolTip(objectName);
  return spin;
}

void AircraftPositionPanel::setAutopilotEnabled(bool enabled) {
  if (m_autopilotEnabled == enabled)
    return;

  m_autopilotEnabled = enabled;
  if (m_autopilotEnabled) {
    ProjectManager::instance()->recordObjectEdit();
    m_preciseX = m_xSpin->value();
    m_preciseY = m_ySpin->value();
    m_flightSceneRevision = ProjectManager::instance()->sceneRevision();
    m_flightClock.start();
    m_autopilotTimer->start();
  } else {
    m_autopilotTimer->stop();
  }
  syncAutopilotButtons();
  syncScreen();
}

void AircraftPositionPanel::advanceAutopilot() {
  if (!m_autopilotEnabled || !m_threeDViewAvailable)
    return;
  const double radians =
      qDegreesToRadians(static_cast<double>(m_courseSpin->value()));
  const double seconds = m_flightClock.nsecsElapsed() / 1e9;
  m_flightClock.restart();
  const double step = Layer3dFormat::movementInPoints(kAutopilotSpeedKmh, seconds);
  const double dx = std::abs(std::cos(radians)) < 1e-12 ? 0 : std::cos(radians);
  const double dy = std::abs(std::sin(radians)) < 1e-12 ? 0 : std::sin(radians);
  const double nextX = m_preciseX + dx * step;
  const double nextY = m_preciseY + dy * step;
  m_preciseX = Layer3dFormat::wrappedPosition(nextX);
  m_preciseY = Layer3dFormat::wrappedPosition(nextY);

  m_updatingControls = true;
  const QSignalBlocker xBlocker(m_xSpin);
  const QSignalBlocker yBlocker(m_ySpin);
  m_xSpin->setValue(m_preciseX);
  m_ySpin->setValue(m_preciseY);
  m_updatingControls = false;
  syncScreen();
}

bool AircraftPositionPanel::eventFilter(QObject *watched, QEvent *event) {
  // В полях ввода стрелки, знаки и Shift сохраняют обычную работу с текстом.
  if (qobject_cast<QLineEdit *>(watched) || qobject_cast<QAbstractSpinBox *>(watched))
    return BasePanel::eventFilter(watched, event);
  if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
    auto *key = static_cast<QKeyEvent *>(event);
    if (handleControlKey(key, event->type() == QEvent::KeyPress)) {
      key->accept();
      return true;
    }
  }
  return BasePanel::eventFilter(watched, event);
}

bool AircraftPositionPanel::handleControlKey(QKeyEvent *event, bool apply) {
  // Не перехватываем и событие, которое поле ввода могло передать родителю.
  const QWidget *focus = QApplication::focusWidget();
  if (qobject_cast<const QLineEdit *>(focus) || qobject_cast<const QAbstractSpinBox *>(focus))
    return false;
  if (!m_threeDViewAvailable || (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))
    return false;
  const bool fast = event->modifiers().testFlag(Qt::ShiftModifier);
  const int key = event->key();
  if (key == Qt::Key_Left || key == Qt::Key_Right) {
    if (apply) adjustCourse(key == Qt::Key_Right ? 1 : -1, fast);
  } else if (key == Qt::Key_Up || key == Qt::Key_Down) {
    if (apply) adjustScreenY(key == Qt::Key_Up ? 1 : -1, fast);
  } else if (key == Qt::Key_Plus || key == Qt::Key_Equal || key == Qt::Key_Minus) {
    if (apply) adjustAltitude(key == Qt::Key_Minus ? -1 : 1, fast);
  } else return false;
  return true;
}

void AircraftPositionPanel::adjustCourse(int direction, bool fast) {
  m_courseSpin->setValue(normalizedCourse(m_courseSpin->value()
      + direction * (fast ? kFastCourseStep : kCourseStep)));
}

void AircraftPositionPanel::adjustScreenY(int direction, bool fast) {
  m_screenYSpin->setValue(m_screenYSpin->value() + direction * (fast ? kFastScreenYStep : 1));
}

void AircraftPositionPanel::adjustAltitude(int direction, bool fast) {
  m_altitudeSpin->setValue(m_altitudeSpin->value()
      + direction * (fast ? kFastAltitudeStep : Layer3dFormat::heightStep));
}

void AircraftPositionPanel::syncAutopilotButtons() {
  const QSignalBlocker blocker(m_autopilotButton);
  m_autopilotButton->setChecked(m_autopilotEnabled);
  m_autopilotButton->setIcon(
      QIcon(m_autopilotEnabled ? kAutopilotOnIcon : kAutopilotOffIcon));
}

void AircraftPositionPanel::syncScreen() {
  m_screenWidget->setState(m_xSpin->value(), m_ySpin->value(),
                           m_altitudeSpin->value(), m_courseSpin->value());
  if (!m_syncingFromProject && ProjectManager::instance()->hasThreeDViewObject())
    ProjectManager::instance()->setAircraftState(m_xSpin->value(), m_ySpin->value(),
                                                m_altitudeSpin->value(), m_courseSpin->value(),
                                                m_screenYSpin->value(), !m_autopilotEnabled);
}

void AircraftPositionPanel::syncFromProject() {
  if (m_syncingFromProject)
    return;
  m_syncingFromProject = true;
  auto *object = ProjectManager::instance()->threeDViewObject();
  setThreeDViewAvailable(object != nullptr);
  const QSignalBlocker xBlocker(m_xSpin);
  const QSignalBlocker yBlocker(m_ySpin);
  const QSignalBlocker altitudeBlocker(m_altitudeSpin);
  const QSignalBlocker courseBlocker(m_courseSpin);
  const QSignalBlocker screenYBlocker(m_screenYSpin);
  const double x = object ? object->aircraftX() : 0;
  const double y = object ? object->aircraftY() : 0;
  const double altitude = object ? object->aircraftAltitude() : 0.0;
  // После отмены, повтора или загрузки другого состояния старый таймер
  // не должен перезаписать координаты остатками предыдущего полета.
  if (m_autopilotEnabled && (ProjectManager::instance()->sceneRevision() != m_flightSceneRevision
                           || std::abs(x - m_xSpin->value()) > 1.0 / 64
                           || std::abs(y - m_ySpin->value()) > 1.0 / 64))
    setAutopilotEnabled(false);
  // Сохраняем импортированную высоту без изменения ее сырого представления.
  m_altitudeSpin->setRange(qMin(kMinAltitude, altitude), qMax(kMaxAltitude, altitude));
  m_xSpin->setValue(x);
  m_ySpin->setValue(y);
  m_altitudeSpin->setValue(altitude);
  m_courseSpin->setValue(object ? object->aircraftCourse() : 0);
  m_screenYSpin->setValue(object ? object->aircraftScreenY() : Layer3dFormat::defaultScreenY);
  if (!m_autopilotEnabled) {
    m_preciseX = x;
    m_preciseY = y;
  }
  m_altitudeSlider->setValue(altitude);
  m_courseKnob->setCourse(m_courseSpin->value());
  syncScreen();
  m_syncingFromProject = false;
}

void AircraftPositionPanel::updateAvailabilityOverlay() {
  const bool available = m_threeDViewAvailable;
  const QList<QWidget *> controls = {
      m_xSpin, m_ySpin, m_altitudeSpin, m_courseSpin, m_altitudeSlider,
      m_courseKnob, m_autopilotButton, m_courseControlArea, m_altitudeControlArea,
      m_screenYSpin, m_screenYUpButton, m_screenYDownButton};
  for (QWidget *control : controls) {
    if (control)
      control->setEnabled(available);
  }

  if (!m_unavailableOverlay)
    return;

  m_unavailableOverlay->setGeometry(rect().adjusted(14, 14, -14, -14));
  m_unavailableOverlay->setVisible(!available);
  if (!available)
    m_unavailableOverlay->raise();
}
