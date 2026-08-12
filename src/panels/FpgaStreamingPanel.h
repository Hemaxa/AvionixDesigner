//FpgaStreamingPanel - панель выбора способа загрузки пакетов ПЛИС

#pragma once

#include "BasePanel.h"
#include "fpga/packets/FpgaPacket.h"
#include "fpga/transport/FpgaUartTransport.h"

class QBoxLayout;
class QFrame;
class QResizeEvent;
class QTimer;
class QToolButton;

class FpgaStreamingPanel : public BasePanel
{
    Q_OBJECT

public:
    explicit FpgaStreamingPanel(QWidget *parent = nullptr);

signals:
    void simulatorBundleReady(const FpgaPacketBundle &bundle);
    void simulatorLaunchRequested();
    void simulationActiveChanged(bool active);

public slots:
    void startStreaming();
    void stopStreaming();

    void toggleBinMode();
    void toggleSimulatorMode();
    void toggleUartMode();

private slots:
    void modeSelectionChanged();
    void transmitUartFrame();

private:
    void resizeEvent(QResizeEvent *event) override;
    bool compilePackets(bool showError = true);
    bool ensureCompiled();
    bool hasSelectedMode() const;
    bool simulatorModeActive() const;
    bool configureUartStreaming();
    bool sendCurrentBundleViaUart(bool showSuccessMessage);
    void startUartStreaming();
    void stopUartStreaming();
    void setRunning(bool running);
    void invalidatePackets();
    void updateAdaptiveLayout();

    QBoxLayout *m_layout = nullptr;
    QBoxLayout *m_modeLayout = nullptr;
    QBoxLayout *m_buttonLayout = nullptr;
    QFrame *m_separator = nullptr;

    QToolButton *m_binButton = nullptr;
    QToolButton *m_simulatorButton = nullptr;
    QToolButton *m_uartButton = nullptr;
    QToolButton *m_startButton = nullptr;
    QToolButton *m_stopButton = nullptr;

    FpgaPacketBundle m_bundle;
    QTimer *m_uartTimer = nullptr;
    FpgaUartConfig m_uartConfig;
    int m_uartIntervalMs = 2000;
    bool m_hasBundle = false;
    bool m_running = false;
    bool m_uartStreaming = false;
    bool m_uartSending = false;
    bool m_verticalLayout = false;
};
