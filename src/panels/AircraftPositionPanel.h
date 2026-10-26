//AircraftPositionPanel - панель позиционирования самолета в пространстве

#pragma once

#include "BasePanel.h"

class QGridLayout;
class QHBoxLayout;
class QLabel;
class QFrame;
class QResizeEvent;
class QSpinBox;
class QTimer;
class QToolButton;
class QWidget;

class AircraftPositionPanel : public BasePanel
{
    Q_OBJECT

public:
    explicit AircraftPositionPanel(QWidget *parent = nullptr);
    void setThreeDViewAvailable(bool available);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void setAutopilotEnabled(bool enabled);
    void advanceAutopilot();

private:
    class ScreenWidget;
    class AltitudeSliderWidget;
    class CourseKnobWidget;

    void syncScreen();
    void syncFromProject();
    void syncAutopilotButtons();
    void applyControlScale();
    void updateAvailabilityOverlay();
    QSpinBox* createValueSpin(const QString &objectName, int min, int max, int value, const QString &suffix = QString());

    ScreenWidget *m_screenWidget = nullptr;
    AltitudeSliderWidget *m_altitudeSlider = nullptr;
    CourseKnobWidget *m_courseKnob = nullptr;
    QSpinBox *m_xSpin = nullptr;
    QSpinBox *m_ySpin = nullptr;
    QSpinBox *m_altitudeSpin = nullptr;
    QSpinBox *m_courseSpin = nullptr;
    QToolButton *m_autopilotButton = nullptr;
    QTimer *m_autopilotTimer = nullptr;
    QWidget *m_courseControlArea = nullptr;
    QWidget *m_altitudeControlArea = nullptr;
    QFrame *m_unavailableOverlay = nullptr;
    QHBoxLayout *m_mainLayout = nullptr;
    QGridLayout *m_controlsLayout = nullptr;

    double m_preciseX = 0.0;
    double m_preciseY = 0.0;
    double m_appliedControlScale = 0.0;
    bool m_autopilotEnabled = false;
    bool m_updatingControls = false;
    bool m_syncingFromProject = false;
    bool m_threeDViewAvailable = false;
};
