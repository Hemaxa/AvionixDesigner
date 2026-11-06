// FpgaStreamingPanel - панель выбора способа загрузки пакетов ПЛИС.

#pragma once

#include "BasePanel.h"
#include "fpga/packets/FpgaPacket.h"
#include "fpga/transport/FpgaUartTransport.h"
#include "fpga/config/FpgaPipelineConfig.h"

class QToolButton;

// FpgaStreamingPanel - панель выбора способа загрузки пакетов ПЛИС.
class FpgaStreamingPanel : public BasePanel
{
    Q_OBJECT

public:
    explicit FpgaStreamingPanel(QWidget *parent = nullptr);
    ~FpgaStreamingPanel() override;

signals:
    void simulatorBundleReady(const FpgaPacketBundle &bundle);
    void simulatorLaunchRequested();
    void simulationActiveChanged(bool active);

public slots:
    void startStreaming();
    void stopStreaming();

    void toggleUartMode();
    void toggleBinMode();
    void toggleSimulatorMode();
    void exportConfiguration();

private slots:
    void modeSelectionChanged();
    void transmitUartFrame();

private:
    bool compilePackets(bool showError = true);
    bool ensureCompiled();
    bool hasSelectedMode() const;
    bool simulatorModeActive() const;
    bool configureUartStreaming();
    void startUartStreaming();
    void stopUartStreaming();
    void setRunning(bool running);
    void invalidatePackets();
    bool saveConfiguration(const QString &path);
    QString defaultConfigurationPath() const;

    QToolButton *m_binButton = nullptr;
    QToolButton *m_simulatorButton = nullptr;
    QToolButton *m_uartButton = nullptr;
    QToolButton *m_startButton = nullptr;
    QToolButton *m_stopButton = nullptr;
    QToolButton *m_exportConfigurationButton = nullptr;
    FpgaPipelineConfig m_uartConfiguration;

    FpgaPacketBundle m_bundle;
    FpgaUartTransport *m_uartTransport = nullptr;
    quint64 m_lastSceneRevision = 0;
    bool m_updateScheduled = false;
    FpgaUartConfig m_uartConfig;
    bool m_hasBundle = false;
    bool m_running = false;
    bool m_uartStreaming = false;
};
