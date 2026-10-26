// AircraftPositionPanel - панель позиционирования самолета в пространстве

#include "AircraftPositionPanel.h"
#include "AircraftPositionMapping.h"
#include "ProjectManager.h"
#include "ThreeDViewObject.h"

#include <QAbstractSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>

namespace {
constexpr int kMinPosition = AircraftPositionMapping::minPosition;
constexpr int kMaxPosition = AircraftPositionMapping::maxPosition;
constexpr int kMinAltitude = 0;
constexpr int kMaxAltitude = 10000;
constexpr int kDefaultAutopilotIntervalMs = 90;
constexpr double kAutopilotStep = 2.5;
constexpr double kMinimumPanelScale = 0.60;
constexpr double kMaximumPanelScale = 1.0;

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

double normalizedCourse(double value) {
  while (value < 0.0)
    value += 360.0;
  while (value >= 360.0)
    value -= 360.0;
  return value;
}

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

QLabel *createCaption(const QString &text, QWidget *parent) {
  auto *label = new QLabel(text, parent);
  label->setObjectName(QStringLiteral("AircraftPositionCaption"));
  label->setAlignment(Qt::AlignCenter);
  return label;
}
} // namespace

class AircraftPositionPanel::ScreenWidget : public QWidget {
public:
  explicit ScreenWidget(QWidget *parent = nullptr)
      : QWidget(parent), m_screenRenderer(kScreenIcon),
        m_aircraftRenderer(kAircraftIcon) {
    setObjectName(QStringLiteral("AircraftPositionScreen"));
    m_screenRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
    m_aircraftRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
    setMinimumSize(340, 340);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  }

  void setState(int x, int y, int altitude, int course, bool autopilotEnabled) {
    m_x = x;
    m_y = y;
    m_altitude = altitude;
    m_course = course;
    m_autopilotEnabled = autopilotEnabled;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(6, 10, 13));

    const QRectF screenRect =
        fittedRect(rect().adjusted(12, 12, -12, -12), QSizeF(1.0, 1.0));
    drawSvg(&painter, m_screenRenderer, screenRect);

    const QPointF aircraftCenter = AircraftPositionMapping::center(screenRect, m_x, m_y);

    const double aircraftHeight =
        screenRect.height() *
        (0.16 + 0.04 * m_altitude / static_cast<double>(kMaxAltitude));
    const double aircraftWidth = aircraftHeight * 0.48;
    const QRectF aircraftRect(-aircraftWidth * 0.5, -aircraftHeight * 0.54,
                              aircraftWidth, aircraftHeight);

    painter.save();
    painter.translate(aircraftCenter);
    painter.rotate(m_course);
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
  int m_x = 0;
  int m_y = 0;
  int m_altitude = 0;
  int m_course = 0;
  bool m_autopilotEnabled = false;
};

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

  void setValue(int value) {
    const int nextValue = std::clamp(value, kMinAltitude, kMaxAltitude);
    if (m_value == nextValue)
      return;
    m_value = nextValue;
    update();
  }

  int value() const { return m_value; }
  void setControlScale(double scale) {
    setFixedSize(qRound(170 * scale), qRound(232 * scale));
    update();
  }
  std::function<void(int)> valueChanged;

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
    painter.setFont(QFont(painter.font().family(),
                          qMax(10, qRound(width() * 0.056)), QFont::DemiBold));
    const double labelX = tickX + width() * 0.15;
    painter.drawText(
        QRectF(labelX, valueRange.top() - 14, width() - labelX, 28),
        Qt::AlignLeft | Qt::AlignVCenter, QString::number(kMaxAltitude));
    painter.drawText(
        QRectF(labelX, valueRange.bottom() - 14, width() - labelX, 28),
        Qt::AlignLeft | Qt::AlignVCenter, QString::number(kMinAltitude));

    const double ratio = (m_value - kMinAltitude) /
                         static_cast<double>(kMaxAltitude - kMinAltitude);
    const double thumbY = valueRange.bottom() - ratio * valueRange.height();
    painter.setPen(
        QPen(QColor(9, 17, 22, 150), 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(trackRect.center().x(), trackRect.top() + 25),
                     QPointF(trackRect.center().x(), trackRect.bottom() - 25));

    const double thumbWidth = trackRect.width() * 1.38;
    const QRectF thumb(trackRect.center().x() - thumbWidth * 0.5,
                       thumbY - thumbWidth * 0.36, thumbWidth,
                       thumbWidth * 0.72);
    drawSvg(&painter, m_thumbRenderer, thumb);
  }

  void mousePressEvent(QMouseEvent *event) override {
    setValueFromY(event->position().y());
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (event->buttons() & Qt::LeftButton)
      setValueFromY(event->position().y());
  }

  void wheelEvent(QWheelEvent *event) override {
    const int direction = event->angleDelta().y() >= 0 ? 1 : -1;
    updateValue(m_value + direction * 100);
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Up) {
      updateValue(m_value + 100);
    } else if (event->key() == Qt::Key_Down) {
      updateValue(m_value - 100);
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
    updateValue(kMinAltitude + static_cast<int>(std::round(
                                   ratio * (kMaxAltitude - kMinAltitude))));
  }

  void updateValue(int value) {
    const int nextValue = std::clamp(value, kMinAltitude, kMaxAltitude);
    if (nextValue == m_value)
      return;
    m_value = nextValue;
    update();
    if (valueChanged)
      valueChanged(m_value);
  }

  QSvgRenderer m_trackRenderer;
  QSvgRenderer m_thumbRenderer;
  int m_value = 0;
};

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

  void setCourse(int course) {
    const int nextCourse =
        static_cast<int>(std::round(normalizedCourse(course)));
    if (m_course == nextCourse)
      return;
    m_course = nextCourse;
    update();
  }

  int course() const { return m_course; }
  void setControlScale(double scale) {
    setFixedSize(qRound(260 * scale), qRound(224 * scale));
    update();
  }
  std::function<void(int)> courseChanged;

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
    painter.rotate(m_course);
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
    painter.setFont(QFont(painter.font().family(),
                          qMax(10, qRound(height() * 0.045)), QFont::DemiBold));
    drawCardinalLabel(&painter, baseRect, QStringLiteral("С 0°"), 0);
    drawCardinalLabel(&painter, baseRect, QStringLiteral("В 90°"), 90);
    drawCardinalLabel(&painter, baseRect, QStringLiteral("Ю 180°"), 180);
    drawCardinalLabel(&painter, baseRect, QStringLiteral("З 270°"), 270);
  }

  void mousePressEvent(QMouseEvent *event) override {
    updateFromPosition(event->position());
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (event->buttons() & Qt::LeftButton)
      updateFromPosition(event->position());
  }

  void wheelEvent(QWheelEvent *event) override {
    const int direction = event->angleDelta().y() >= 0 ? 1 : -1;
    updateCourse(m_course +
                 direction * (event->modifiers() & Qt::ShiftModifier ? 10 : 1));
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Left) {
      updateCourse(m_course - 1);
    } else if (event->key() == Qt::Key_Right) {
      updateCourse(m_course + 1);
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
    updateCourse(static_cast<int>(std::round(normalizedCourse(
        qRadiansToDegrees(std::atan2(delta.x(), -delta.y()))))));
  }

  void updateCourse(int course) {
    const int nextCourse =
        static_cast<int>(std::round(normalizedCourse(course)));
    if (nextCourse == m_course)
      return;
    m_course = nextCourse;
    update();
    if (courseChanged)
      courseChanged(m_course);
  }

  QSvgRenderer m_baseRenderer;
  QSvgRenderer m_pointerRenderer;
  int m_course = 0;
};

AircraftPositionPanel::AircraftPositionPanel(QWidget *parent)
    : BasePanel(parent) {
  setPanelName(QStringLiteral("AircraftPositionPanel"));
  setMinimumSize(700, 360);

  m_screenWidget = new ScreenWidget(this);
  m_xSpin = createValueSpin(QStringLiteral("AircraftXSpin"), kMinPosition,
                            kMaxPosition, 0);
  m_ySpin = createValueSpin(QStringLiteral("AircraftYSpin"), kMinPosition,
                            kMaxPosition, 0);
  m_altitudeSpin =
      createValueSpin(QStringLiteral("AircraftAltitudeSpin"), kMinAltitude,
                      kMaxAltitude, 0, QStringLiteral(" м"));
  m_courseSpin = createValueSpin(QStringLiteral("AircraftCourseSpin"), 0, 359,
                                 0, QStringLiteral("°"));
  m_altitudeSlider = new AltitudeSliderWidget(this);
  m_courseKnob = new CourseKnobWidget(this);
  m_courseControlArea = new QWidget(this);
  m_altitudeControlArea = new QWidget(this);
  m_autopilotButton = new QToolButton(this);
  m_autopilotTimer = new QTimer(this);
  m_autopilotTimer->setInterval(kDefaultAutopilotIntervalMs);

  m_autopilotButton->setObjectName(QStringLiteral("AircraftAutopilotButton"));
  m_autopilotButton->setCheckable(true);
  m_autopilotButton->setAutoRaise(true);
  m_autopilotButton->setFixedSize(92, 54);
  m_autopilotButton->setIconSize(QSize(92, 54));
  m_autopilotButton->setToolTip(QStringLiteral("Автопилот"));

  auto *coordinateLayout = new QGridLayout();
  coordinateLayout->setContentsMargins(0, 0, 0, 0);
  coordinateLayout->setHorizontalSpacing(8);
  coordinateLayout->setVerticalSpacing(6);
  coordinateLayout->addWidget(createCaption(QStringLiteral("Координаты"), this),
                              0, 0, 1, 2);
  coordinateLayout->addWidget(m_xSpin, 1, 0, Qt::AlignCenter);
  coordinateLayout->addWidget(m_ySpin, 1, 1, Qt::AlignCenter);

  auto *autopilotLayout = new QVBoxLayout();
  autopilotLayout->setContentsMargins(0, 0, 0, 0);
  autopilotLayout->setSpacing(6);
  autopilotLayout->addWidget(createCaption(QStringLiteral("Автопилот"), this),
                             0, Qt::AlignCenter);
  autopilotLayout->addWidget(m_autopilotButton, 0, Qt::AlignCenter);

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
  m_controlsLayout->setVerticalSpacing(22);
  m_controlsLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  m_controlsLayout->addLayout(coordinateLayout, 0, 0, 1, 2,
                              Qt::AlignHCenter | Qt::AlignTop);
  m_controlsLayout->addLayout(autopilotLayout, 0, 2, 1, 1,
                              Qt::AlignHCenter | Qt::AlignTop);
  m_controlsLayout->addLayout(courseLayout, 1, 0, 1, 2,
                              Qt::AlignHCenter | Qt::AlignTop);
  m_controlsLayout->addLayout(altitudeLayout, 1, 2, 1, 1,
                              Qt::AlignHCenter | Qt::AlignTop);
  m_controlsLayout->setColumnMinimumWidth(0, 82);
  m_controlsLayout->setColumnMinimumWidth(1, 82);
  m_controlsLayout->setColumnMinimumWidth(2, 120);

  m_mainLayout = new QHBoxLayout(this);
  m_mainLayout->setContentsMargins(8, 8, 8, 8);
  m_mainLayout->setSpacing(12);
  m_mainLayout->addWidget(m_screenWidget, 1);
  m_mainLayout->addLayout(m_controlsLayout, 0);

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

  connect(m_xSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int value) {
            if (!m_updatingControls)
              m_preciseX = value;
            syncScreen();
          });
  connect(m_ySpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int value) {
            if (!m_updatingControls)
              m_preciseY = value;
            syncScreen();
          });
  connect(m_altitudeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int value) {
            m_altitudeSlider->setValue(value);
            syncScreen();
          });
  connect(m_courseSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int value) {
            m_courseKnob->setCourse(value);
            syncScreen();
          });
  connect(m_autopilotButton, &QToolButton::clicked, this,
          [this](bool checked) { setAutopilotEnabled(checked); });
  connect(m_autopilotTimer, &QTimer::timeout, this,
          &AircraftPositionPanel::advanceAutopilot);

  m_altitudeSlider->valueChanged = [this](int value) {
    QSignalBlocker blocker(m_altitudeSpin);
    m_altitudeSpin->setValue(value);
    syncScreen();
  };
  m_courseKnob->courseChanged = [this](int course) {
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
  const double widthScale = width() / 820.0;
  const double heightScale = height() / 450.0;
  const double rawScale = std::clamp(std::min(widthScale, heightScale),
                                     kMinimumPanelScale, kMaximumPanelScale);
  double scale = 0.88;
  if (rawScale < 0.68) {
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
    m_controlsLayout->setVerticalSpacing(qRound(22 * layoutScale));
    m_controlsLayout->setColumnMinimumWidth(0, qRound(82 * scale));
    m_controlsLayout->setColumnMinimumWidth(1, qRound(82 * scale));
    m_controlsLayout->setColumnMinimumWidth(2, qRound(120 * scale));
  }

  if (qFuzzyCompare(m_appliedControlScale, scale))
    return;
  m_appliedControlScale = scale;

  const int compactFieldHeight = qRound(std::max(30.0, 32.0 * scale));
  const QSize coordinateFieldSize(qRound(std::max(76.0, 84.0 * scale)),
                                  compactFieldHeight);
  const QSize courseFieldSize(qRound(std::max(78.0, 84.0 * scale)),
                              compactFieldHeight);
  const QSize altitudeFieldSize(qRound(std::max(106.0, 112.0 * scale)),
                                compactFieldHeight);
  m_xSpin->setFixedSize(coordinateFieldSize);
  m_ySpin->setFixedSize(coordinateFieldSize);
  m_courseSpin->setFixedSize(courseFieldSize);
  m_altitudeSpin->setFixedSize(altitudeFieldSize);

  m_courseKnob->setControlScale(scale);
  m_altitudeSlider->setControlScale(scale);
  m_courseControlArea->setFixedSize(qRound(260 * scale), qRound(232 * scale));
  m_altitudeControlArea->setFixedSize(qRound(170 * scale), qRound(232 * scale));

  const QSize autopilotSize(qRound(92 * scale), qRound(54 * scale));
  m_autopilotButton->setFixedSize(autopilotSize);
  m_autopilotButton->setIconSize(autopilotSize);

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
}

QSpinBox *AircraftPositionPanel::createValueSpin(const QString &objectName,
                                                 int min, int max, int value,
                                                 const QString &suffix) {
  auto *spin = new QSpinBox(this);
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
    m_autopilotTimer->start();
  } else {
    m_autopilotTimer->stop();
  }
  syncAutopilotButtons();
  syncScreen();
}

void AircraftPositionPanel::advanceAutopilot() {
  const double radians =
      qDegreesToRadians(static_cast<double>(m_courseSpin->value()));
  m_preciseX += std::sin(radians) * kAutopilotStep;
  m_preciseY += std::cos(radians) * kAutopilotStep;

  if (m_preciseX > kMaxPosition)
    m_preciseX = kMinPosition;
  if (m_preciseX < kMinPosition)
    m_preciseX = kMaxPosition;
  if (m_preciseY > kMaxPosition)
    m_preciseY = kMinPosition;
  if (m_preciseY < kMinPosition)
    m_preciseY = kMaxPosition;

  m_updatingControls = true;
  const QSignalBlocker xBlocker(m_xSpin);
  const QSignalBlocker yBlocker(m_ySpin);
  m_xSpin->setValue(static_cast<int>(std::round(m_preciseX)));
  m_ySpin->setValue(static_cast<int>(std::round(m_preciseY)));
  m_updatingControls = false;
  syncScreen();
}

void AircraftPositionPanel::syncAutopilotButtons() {
  const QSignalBlocker blocker(m_autopilotButton);
  m_autopilotButton->setChecked(m_autopilotEnabled);
  m_autopilotButton->setIcon(
      QIcon(m_autopilotEnabled ? kAutopilotOnIcon : kAutopilotOffIcon));
}

void AircraftPositionPanel::syncScreen() {
  m_screenWidget->setState(m_xSpin->value(), m_ySpin->value(),
                           m_altitudeSpin->value(), m_courseSpin->value(),
                           m_autopilotEnabled);
  if (!m_syncingFromProject && ProjectManager::instance()->hasThreeDViewObject())
    ProjectManager::instance()->setAircraftState(m_xSpin->value(), m_ySpin->value(),
                                                m_altitudeSpin->value(), m_courseSpin->value(),
                                                !m_autopilotEnabled);
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
  const int x = object ? object->aircraftX() : 0;
  const int y = object ? object->aircraftY() : 0;
  const int altitude = object ? object->aircraftAltitude() : 0;
  // Imported raw coordinates may exceed the panel's normal movement range.
  // Display them without silently changing the stored position.
  m_xSpin->setRange(qMin(kMinPosition, x), qMax(kMaxPosition, x));
  m_ySpin->setRange(qMin(kMinPosition, y), qMax(kMaxPosition, y));
  m_altitudeSpin->setRange(qMin(kMinAltitude, altitude), qMax(kMaxAltitude, altitude));
  m_xSpin->setValue(x);
  m_ySpin->setValue(y);
  m_altitudeSpin->setValue(altitude);
  m_courseSpin->setValue(object ? object->aircraftCourse() : 0);
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
      m_courseKnob, m_autopilotButton, m_courseControlArea, m_altitudeControlArea};
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
