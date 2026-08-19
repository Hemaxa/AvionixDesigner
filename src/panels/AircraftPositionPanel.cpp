//AircraftPositionPanel - панель позиционирования самолета в пространстве

#include "AircraftPositionPanel.h"

#include <QAbstractSpinBox>
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
#include <QSvgRenderer>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>

namespace {
constexpr int kMinPosition = -1000;
constexpr int kMaxPosition = 1000;
constexpr int kMinAltitude = 0;
constexpr int kMaxAltitude = 10000;
constexpr int kDefaultAutopilotIntervalMs = 90;
constexpr double kAutopilotStep = 2.5;
constexpr double kMinimumPanelScale = 0.72;
constexpr double kMaximumPanelScale = 1.12;

const QString kInputFieldIcon = QStringLiteral(":/icons/icons/aircraft-position/input-field.svg");
const QString kSliderThumbIcon = QStringLiteral(":/icons/icons/aircraft-position/slider-thumb.svg");
const QString kScreenIcon = QStringLiteral(":/icons/icons/aircraft-position/screen.svg");
const QString kAircraftIcon = QStringLiteral(":/icons/icons/aircraft-position/aircraft.svg");
const QString kAltitudeTrackIcon = QStringLiteral(":/icons/icons/aircraft-position/altitude-track.svg");
const QString kCourseBaseIcon = QStringLiteral(":/icons/icons/aircraft-position/course-base.svg");
const QString kCoursePointerIcon = QStringLiteral(":/icons/icons/aircraft-position/course-pointer.svg");
const QString kAutopilotOffIcon = QStringLiteral(":/icons/icons/aircraft-position/autopilot-button-off.svg");
const QString kAutopilotOnIcon = QStringLiteral(":/icons/icons/aircraft-position/autopilot-button-on.svg");

double normalizedCourse(double value)
{
    while (value < 0.0)
        value += 360.0;
    while (value >= 360.0)
        value -= 360.0;
    return value;
}

QRectF fittedRect(const QRectF &bounds, const QSizeF &aspect)
{
    if (bounds.isEmpty() || aspect.isEmpty())
        return bounds;

    const double targetRatio = aspect.width() / aspect.height();
    double width = bounds.width();
    double height = width / targetRatio;
    if (height > bounds.height()) {
        height = bounds.height();
        width = height * targetRatio;
    }

    return QRectF(
        bounds.center().x() - width * 0.5,
        bounds.center().y() - height * 0.5,
        width,
        height
    );
}

void drawSvg(QPainter *painter, QSvgRenderer &renderer, const QRectF &target)
{
    if (renderer.isValid())
        renderer.render(painter, target);
}

QLabel* createCaption(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("AircraftPositionCaption"));
    label->setAlignment(Qt::AlignCenter);
    return label;
}

class ValueSpinBox : public QSpinBox
{
public:
    explicit ValueSpinBox(QWidget *parent = nullptr)
        : QSpinBox(parent)
        , m_backgroundRenderer(kInputFieldIcon)
    {
        m_backgroundRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
        setStyleSheet(QStringLiteral(
            "QSpinBox { background: transparent; border: none; color: #edf7fb; font-size: 20px; font-weight: 700; padding: 0 12px; }"
            "QSpinBox::up-button, QSpinBox::down-button { width: 0px; height: 0px; border: none; }"
            "QSpinBox:focus { color: #ffffff; }"
        ));
        if (lineEdit()) {
            lineEdit()->setStyleSheet(QStringLiteral("background: transparent; border: none; color: #edf7fb; selection-background-color: #00c8f0;"));
            lineEdit()->setAlignment(Qt::AlignCenter);
            lineEdit()->setFrame(false);
        }
    }

    void setDisplayScale(double scale)
    {
        const int fontSize = qRound(20 * scale);
        setStyleSheet(QStringLiteral(
            "QSpinBox { background: transparent; border: none; color: #edf7fb; font-size: %1px; font-weight: 700; padding: 0 %2px; }"
            "QSpinBox::up-button, QSpinBox::down-button { width: 0px; height: 0px; border: none; }"
            "QSpinBox:focus { color: #ffffff; }"
        ).arg(fontSize).arg(qRound(12 * scale)));
        if (lineEdit()) {
            lineEdit()->setStyleSheet(QStringLiteral("background: transparent; border: none; color: #edf7fb; selection-background-color: #00c8f0;"));
            lineEdit()->setAlignment(Qt::AlignCenter);
            lineEdit()->setFrame(false);
            QFont font = lineEdit()->font();
            font.setPixelSize(fontSize);
            font.setWeight(QFont::Bold);
            lineEdit()->setFont(font);
            updateEditorGeometry();
        }
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QSpinBox::resizeEvent(event);
        updateEditorGeometry();
    }

    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        drawSvg(&painter, m_backgroundRenderer, rect());
        QSpinBox::paintEvent(event);
    }

private:
    void updateEditorGeometry()
    {
        if (!lineEdit())
            return;

        const int horizontalInset = qMax(10, qRound(width() * 0.12));
        const int verticalInset = qMax(6, qRound(height() * 0.18));
        lineEdit()->setGeometry(rect().adjusted(horizontalInset, verticalInset, -horizontalInset, -verticalInset));
    }

    QSvgRenderer m_backgroundRenderer;
};
}

class AircraftPositionPanel::ScreenWidget : public QWidget
{
public:
    explicit ScreenWidget(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_screenRenderer(kScreenIcon)
        , m_aircraftRenderer(kAircraftIcon)
    {
        setObjectName(QStringLiteral("AircraftPositionScreen"));
        m_screenRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
        m_aircraftRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
        setMinimumSize(360, 360);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setState(int x, int y, int altitude, int course, bool autopilotEnabled)
    {
        m_x = x;
        m_y = y;
        m_altitude = altitude;
        m_course = course;
        m_autopilotEnabled = autopilotEnabled;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.fillRect(rect(), QColor(6, 10, 13));

        const QRectF screenRect = fittedRect(rect().adjusted(12, 12, -12, -12), QSizeF(1.0, 1.0));
        drawSvg(&painter, m_screenRenderer, screenRect);

        painter.save();
        painter.setPen(QPen(QColor(237, 247, 251, 120), 1));
        painter.drawText(screenRect.adjusted(24, 20, -24, -20), Qt::AlignTop | Qt::AlignLeft, QStringLiteral("ЭКРАН"));
        painter.restore();

        const double normX = std::clamp(static_cast<double>(m_x) / static_cast<double>(kMaxPosition), -1.0, 1.0);
        const double normY = std::clamp(static_cast<double>(m_y) / static_cast<double>(kMaxPosition), -1.0, 1.0);
        const QPointF aircraftCenter(
            screenRect.center().x() + normX * screenRect.width() * 0.38,
            screenRect.center().y() - normY * screenRect.height() * 0.38
        );

        const double aircraftHeight = screenRect.height() * (0.16 + 0.04 * m_altitude / static_cast<double>(kMaxAltitude));
        const double aircraftWidth = aircraftHeight * 0.48;
        const QRectF aircraftRect(-aircraftWidth * 0.5, -aircraftHeight * 0.54, aircraftWidth, aircraftHeight);

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

class AircraftPositionPanel::AltitudeSliderWidget : public QWidget
{
public:
    explicit AltitudeSliderWidget(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_trackRenderer(kAltitudeTrackIcon)
        , m_thumbRenderer(kSliderThumbIcon)
    {
        setObjectName(QStringLiteral("AircraftAltitudeSlider"));
        m_trackRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
        m_thumbRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
        setMinimumSize(172, 281);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        setFocusPolicy(Qt::StrongFocus);
    }

    void setValue(int value)
    {
        const int nextValue = std::clamp(value, kMinAltitude, kMaxAltitude);
        if (m_value == nextValue)
            return;
        m_value = nextValue;
        update();
    }

    int value() const { return m_value; }
    void setControlScale(double scale)
    {
        setFixedSize(qRound(220 * scale), qRound(360 * scale));
        update();
    }
    std::function<void(int)> valueChanged;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const double marginY = height() * 0.095;
        const double trackWidth = width() * 0.34;
        const QRectF trackRect(width() * 0.30, marginY, trackWidth, height() - marginY * 2.0);
        const QRectF valueRange(trackRect.left(), trackRect.top() + trackRect.height() * 0.12, trackRect.width(), trackRect.height() * 0.76);
        drawSvg(&painter, m_trackRenderer, trackRect);

        const double tickX = trackRect.right() + width() * 0.12;
        painter.setPen(QPen(QColor(237, 247, 251, 220), qMax(3, qRound(width() * 0.022)), Qt::SolidLine, Qt::RoundCap));
        const int tickCount = 10;
        for (int i = 0; i <= tickCount; ++i) {
            const double t = static_cast<double>(i) / tickCount;
            const double y = valueRange.top() + t * valueRange.height();
            const double tickLength = (i % 5 == 0) ? width() * 0.12 : width() * 0.08;
            painter.drawLine(QPointF(tickX, y), QPointF(tickX + tickLength, y));
        }

        painter.setPen(QColor(237, 247, 251));
        painter.setFont(QFont(painter.font().family(), qMax(11, qRound(width() * 0.062)), QFont::DemiBold));
        const double labelX = tickX + width() * 0.16;
        painter.drawText(QRectF(labelX, valueRange.top() - 14, width() - labelX, 28), Qt::AlignLeft | Qt::AlignVCenter, QString::number(kMaxAltitude));
        painter.drawText(QRectF(labelX, valueRange.bottom() - 14, width() - labelX, 28), Qt::AlignLeft | Qt::AlignVCenter, QString::number(kMinAltitude));

        const double ratio = (m_value - kMinAltitude) / static_cast<double>(kMaxAltitude - kMinAltitude);
        const double thumbY = valueRange.bottom() - ratio * valueRange.height();
        painter.setPen(QPen(QColor(9, 17, 22, 150), 2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(trackRect.center().x(), trackRect.top() + 25), QPointF(trackRect.center().x(), trackRect.bottom() - 25));

        const double thumbWidth = trackRect.width() * 1.38;
        const QRectF thumb(trackRect.center().x() - thumbWidth * 0.5, thumbY - thumbWidth * 0.36, thumbWidth, thumbWidth * 0.72);
        drawSvg(&painter, m_thumbRenderer, thumb);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        setValueFromY(event->position().y());
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (event->buttons() & Qt::LeftButton)
            setValueFromY(event->position().y());
    }

    void wheelEvent(QWheelEvent *event) override
    {
        const int direction = event->angleDelta().y() >= 0 ? 1 : -1;
        updateValue(m_value + direction * 100);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Up) {
            updateValue(m_value + 100);
        } else if (event->key() == Qt::Key_Down) {
            updateValue(m_value - 100);
        } else {
            QWidget::keyPressEvent(event);
        }
    }

private:
    void setValueFromY(double y)
    {
        const double marginY = height() * 0.095;
        const double trackWidth = width() * 0.34;
        const QRectF trackRect(width() * 0.30, marginY, trackWidth, height() - marginY * 2.0);
        const QRectF valueRange(trackRect.left(), trackRect.top() + trackRect.height() * 0.12, trackRect.width(), trackRect.height() * 0.76);
        const double ratio = std::clamp((valueRange.bottom() - y) / valueRange.height(), 0.0, 1.0);
        updateValue(kMinAltitude + static_cast<int>(std::round(ratio * (kMaxAltitude - kMinAltitude))));
    }

    void updateValue(int value)
    {
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

class AircraftPositionPanel::CourseKnobWidget : public QWidget
{
public:
    explicit CourseKnobWidget(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_baseRenderer(kCourseBaseIcon)
        , m_pointerRenderer(kCoursePointerIcon)
    {
        setObjectName(QStringLiteral("AircraftCourseKnob"));
        m_baseRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
        m_pointerRenderer.setAspectRatioMode(Qt::KeepAspectRatio);
        setMinimumSize(218, 218);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFocusPolicy(Qt::StrongFocus);
    }

    void setCourse(int course)
    {
        const int nextCourse = static_cast<int>(std::round(normalizedCourse(course)));
        if (m_course == nextCourse)
            return;
        m_course = nextCourse;
        update();
    }

    int course() const { return m_course; }
    void setControlScale(double scale)
    {
        const int size = qRound(280 * scale);
        setFixedSize(size, size);
        update();
    }
    std::function<void(int)> courseChanged;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRectF outer = rect().adjusted(width() * 0.19, height() * 0.19, -width() * 0.19, -height() * 0.19);
        drawSvg(&painter, m_baseRenderer, outer);

        painter.save();
        painter.translate(outer.center());
        painter.rotate(m_course);
        const double pointerSize = outer.width() * 0.84;
        drawSvg(&painter, m_pointerRenderer, QRectF(-pointerSize * 0.5, -pointerSize * 0.5, pointerSize, pointerSize));
        painter.restore();

        for (int degrees = 0; degrees < 360; degrees += 15) {
            const bool major = degrees % 45 == 0;
            const double angle = qDegreesToRadians(static_cast<double>(degrees));
            const QPointF direction(std::sin(angle), -std::cos(angle));
            const double radius = outer.width() * 0.5;
            const double startRadius = radius + width() * 0.018;
            const double endRadius = startRadius + (major ? width() * 0.060 : width() * 0.036);
            painter.setPen(QPen(QColor(237, 247, 251), major ? qMax(3, qRound(width() * 0.018)) : qMax(2, qRound(width() * 0.010)), Qt::SolidLine, Qt::RoundCap));
            const QPointF p1 = outer.center() + direction * startRadius;
            const QPointF p2 = outer.center() + direction * endRadius;
            painter.drawLine(p1, p2);
        }

        painter.setPen(QColor(237, 247, 251));
        painter.setFont(QFont(painter.font().family(), qMax(10, qRound(width() * 0.046)), QFont::DemiBold));
        drawCardinalLabel(&painter, outer, QStringLiteral("С 0°"), 0);
        drawCardinalLabel(&painter, outer, QStringLiteral("В 90°"), 90);
        drawCardinalLabel(&painter, outer, QStringLiteral("Ю 180°"), 180);
        drawCardinalLabel(&painter, outer, QStringLiteral("З 270°"), 270);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        updateFromPosition(event->position());
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (event->buttons() & Qt::LeftButton)
            updateFromPosition(event->position());
    }

    void wheelEvent(QWheelEvent *event) override
    {
        const int direction = event->angleDelta().y() >= 0 ? 1 : -1;
        updateCourse(m_course + direction * (event->modifiers() & Qt::ShiftModifier ? 10 : 1));
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Left) {
            updateCourse(m_course - 1);
        } else if (event->key() == Qt::Key_Right) {
            updateCourse(m_course + 1);
        } else {
            QWidget::keyPressEvent(event);
        }
    }

private:
    void drawCardinalLabel(QPainter *painter, const QRectF &outer, const QString &text, double degrees)
    {
        QRectF labelRect;
        if (degrees == 0) {
            labelRect = QRectF(outer.center().x() - width() * 0.20, 4, width() * 0.40, height() * 0.11);
        } else if (degrees == 90) {
            labelRect = QRectF(width() - width() * 0.30, outer.center().y() - height() * 0.055, width() * 0.29, height() * 0.11);
        } else if (degrees == 180) {
            labelRect = QRectF(outer.center().x() - width() * 0.23, height() - height() * 0.12, width() * 0.46, height() * 0.11);
        } else {
            labelRect = QRectF(1, outer.center().y() - height() * 0.055, width() * 0.30, height() * 0.11);
        }
        painter->drawText(labelRect, Qt::AlignCenter, text);
    }

    void updateFromPosition(const QPointF &position)
    {
        const QPointF center = rect().center();
        const QPointF delta = position - center;
        updateCourse(static_cast<int>(std::round(normalizedCourse(qRadiansToDegrees(std::atan2(delta.x(), -delta.y()))))));
    }

    void updateCourse(int course)
    {
        const int nextCourse = static_cast<int>(std::round(normalizedCourse(course)));
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
    : BasePanel(parent)
{
    setPanelName(QStringLiteral("AircraftPositionPanel"));
    setMinimumSize(900, 680);

    m_screenWidget = new ScreenWidget(this);
    m_xSpin = createValueSpin(QStringLiteral("AircraftXSpin"), kMinPosition, kMaxPosition, 0);
    m_ySpin = createValueSpin(QStringLiteral("AircraftYSpin"), kMinPosition, kMaxPosition, 0);
    m_altitudeSpin = createValueSpin(QStringLiteral("AircraftAltitudeSpin"), kMinAltitude, kMaxAltitude, 0, QStringLiteral(" м"));
    m_courseSpin = createValueSpin(QStringLiteral("AircraftCourseSpin"), 0, 359, 0, QStringLiteral("°"));
    m_altitudeSlider = new AltitudeSliderWidget(this);
    m_courseKnob = new CourseKnobWidget(this);
    m_autopilotButton = new QToolButton(this);
    m_autopilotTimer = new QTimer(this);
    m_autopilotTimer->setInterval(kDefaultAutopilotIntervalMs);

    m_autopilotButton->setObjectName(QStringLiteral("AircraftAutopilotButton"));
    m_autopilotButton->setCheckable(true);
    m_autopilotButton->setAutoRaise(true);
    m_autopilotButton->setFixedSize(156, 91);
    m_autopilotButton->setIconSize(QSize(156, 91));
    m_autopilotButton->setToolTip(QStringLiteral("Автопилот"));

    auto *coordinateLayout = new QGridLayout();
    coordinateLayout->setContentsMargins(0, 0, 0, 0);
    coordinateLayout->setHorizontalSpacing(36);
    coordinateLayout->setVerticalSpacing(12);
    coordinateLayout->addWidget(createCaption(QStringLiteral("Координаты"), this), 0, 0, 1, 2);
    coordinateLayout->addWidget(m_xSpin, 1, 0, Qt::AlignCenter);
    coordinateLayout->addWidget(m_ySpin, 1, 1, Qt::AlignCenter);

    auto *courseFieldLayout = new QVBoxLayout();
    courseFieldLayout->setContentsMargins(0, 0, 0, 0);
    courseFieldLayout->setSpacing(8);
    courseFieldLayout->addStretch(1);
    courseFieldLayout->addWidget(createCaption(QStringLiteral("Курс"), this), 0, Qt::AlignCenter);
    courseFieldLayout->addWidget(m_courseSpin, 0, Qt::AlignCenter);
    courseFieldLayout->addStretch(1);

    auto *altitudeFieldLayout = new QVBoxLayout();
    altitudeFieldLayout->setContentsMargins(0, 0, 0, 0);
    altitudeFieldLayout->setSpacing(8);
    altitudeFieldLayout->addStretch(1);
    altitudeFieldLayout->addWidget(createCaption(QStringLiteral("Высота"), this), 0, Qt::AlignCenter);
    altitudeFieldLayout->addWidget(m_altitudeSpin, 0, Qt::AlignCenter);
    altitudeFieldLayout->addStretch(1);

    m_controlsLayout = new QGridLayout();
    m_controlsLayout->setContentsMargins(22, 16, 22, 16);
    m_controlsLayout->setHorizontalSpacing(34);
    m_controlsLayout->setVerticalSpacing(22);
    m_controlsLayout->addLayout(coordinateLayout, 0, 0, 1, 2);
    m_controlsLayout->addLayout(courseFieldLayout, 1, 0, 1, 1);
    m_controlsLayout->addWidget(m_courseKnob, 1, 1, Qt::AlignCenter);
    m_controlsLayout->addLayout(altitudeFieldLayout, 2, 0, 1, 1);
    m_controlsLayout->addWidget(m_altitudeSlider, 2, 1, Qt::AlignCenter);
    m_controlsLayout->addWidget(m_autopilotButton, 3, 0, 1, 2, Qt::AlignCenter);
    m_controlsLayout->setColumnMinimumWidth(0, 160);
    m_controlsLayout->setColumnMinimumWidth(1, 280);

    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(16, 16, 16, 16);
    m_mainLayout->setSpacing(24);
    m_mainLayout->addWidget(m_screenWidget, 1);
    m_mainLayout->addLayout(m_controlsLayout, 0);

    connect(m_xSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (!m_updatingControls)
            m_preciseX = value;
        syncScreen();
    });
    connect(m_ySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (!m_updatingControls)
            m_preciseY = value;
        syncScreen();
    });
    connect(m_altitudeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_altitudeSlider->setValue(value);
        syncScreen();
    });
    connect(m_courseSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_courseKnob->setCourse(value);
        syncScreen();
    });
    connect(m_autopilotButton, &QToolButton::clicked, this, [this](bool checked) {
        setAutopilotEnabled(checked);
    });
    connect(m_autopilotTimer, &QTimer::timeout, this, &AircraftPositionPanel::advanceAutopilot);

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
    syncScreen();
    applyControlScale();
}

void AircraftPositionPanel::resizeEvent(QResizeEvent *event)
{
    BasePanel::resizeEvent(event);
    applyControlScale();
}

void AircraftPositionPanel::applyControlScale()
{
    const double widthScale = width() / 1260.0;
    const double heightScale = height() / 760.0;
    const double rawScale = std::clamp(std::min(widthScale, heightScale), kMinimumPanelScale, kMaximumPanelScale);
    double scale = 1.0;
    if (rawScale < 0.80) {
        scale = 0.72;
    } else if (rawScale < 0.92) {
        scale = 0.84;
    } else if (rawScale < 1.08) {
        scale = 1.0;
    } else {
        scale = 1.10;
    }

    const double layoutScale = std::clamp(rawScale, kMinimumPanelScale, kMaximumPanelScale);

    if (m_mainLayout) {
        const int margin = qRound(16 * layoutScale);
        m_mainLayout->setContentsMargins(margin, margin, margin, margin);
        m_mainLayout->setSpacing(qRound(24 * layoutScale));
    }

    if (m_controlsLayout) {
        m_controlsLayout->setContentsMargins(qRound(18 * layoutScale), qRound(12 * layoutScale), qRound(18 * layoutScale), qRound(12 * layoutScale));
        m_controlsLayout->setHorizontalSpacing(qRound(30 * layoutScale));
        m_controlsLayout->setVerticalSpacing(qRound(18 * layoutScale));
        m_controlsLayout->setColumnMinimumWidth(0, qRound(160 * scale));
        m_controlsLayout->setColumnMinimumWidth(1, qRound(280 * scale));
    }

    if (qFuzzyCompare(m_appliedControlScale, scale))
        return;
    m_appliedControlScale = scale;

    const QSize fieldSize(qRound(160 * scale), qRound(92 * scale));
    for (QSpinBox *spin : {m_xSpin, m_ySpin, m_altitudeSpin, m_courseSpin}) {
        spin->setFixedSize(fieldSize);
        if (auto *valueSpin = dynamic_cast<ValueSpinBox*>(spin))
            valueSpin->setDisplayScale(scale);
    }

    m_courseKnob->setControlScale(scale);
    m_altitudeSlider->setControlScale(scale);

    const QSize autopilotSize(qRound(156 * scale), qRound(91 * scale));
    m_autopilotButton->setFixedSize(autopilotSize);
    m_autopilotButton->setIconSize(autopilotSize);

    const int captionSize = qRound(18 * scale);
    const QList<QLabel*> labels = findChildren<QLabel*>(QStringLiteral("AircraftPositionCaption"));
    for (QLabel *label : labels) {
        QFont font = label->font();
        font.setPointSize(qMax(12, captionSize));
        font.setWeight(QFont::Bold);
        label->setFont(font);
    }
}

QSpinBox* AircraftPositionPanel::createValueSpin(const QString &objectName, int min, int max, int value, const QString &suffix)
{
    auto *spin = new ValueSpinBox(this);
    spin->setObjectName(objectName);
    spin->setProperty("aircraftPositionField", true);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setAlignment(Qt::AlignCenter);
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setSuffix(suffix);
    spin->setFixedSize(160, 92);
    spin->setToolTip(objectName);
    return spin;
}

void AircraftPositionPanel::setAutopilotEnabled(bool enabled)
{
    if (m_autopilotEnabled == enabled)
        return;

    m_autopilotEnabled = enabled;
    if (m_autopilotEnabled) {
        m_preciseX = m_xSpin->value();
        m_preciseY = m_ySpin->value();
        m_autopilotTimer->start();
    } else {
        m_autopilotTimer->stop();
    }
    syncAutopilotButtons();
    syncScreen();
}

void AircraftPositionPanel::advanceAutopilot()
{
    const double radians = qDegreesToRadians(static_cast<double>(m_courseSpin->value()));
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
    m_xSpin->setValue(static_cast<int>(std::round(m_preciseX)));
    m_ySpin->setValue(static_cast<int>(std::round(m_preciseY)));
    m_updatingControls = false;
    syncScreen();
}

void AircraftPositionPanel::syncAutopilotButtons()
{
    const QSignalBlocker blocker(m_autopilotButton);
    m_autopilotButton->setChecked(m_autopilotEnabled);
    m_autopilotButton->setIcon(QIcon(m_autopilotEnabled ? kAutopilotOnIcon : kAutopilotOffIcon));
}

void AircraftPositionPanel::syncScreen()
{
    m_screenWidget->setState(
        m_xSpin->value(),
        m_ySpin->value(),
        m_altitudeSpin->value(),
        m_courseSpin->value(),
        m_autopilotEnabled
    );
}
