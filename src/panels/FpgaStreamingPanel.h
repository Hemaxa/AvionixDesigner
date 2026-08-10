//FpgaStreamingPanel - панель выбора способа загрузки пакетов ПЛИС

#pragma once

#include "BasePanel.h"
#include "fpga/packets/FpgaPacket.h"

class QToolButton;

class FpgaStreamingPanel : public BasePanel
{
    Q_OBJECT

public:
    enum class Target
    {
        Bin,
        Simulator,
        Uart
    };

    explicit FpgaStreamingPanel(QWidget *parent = nullptr);

signals:
    void simulatorBundleReady(const FpgaPacketBundle &bundle);
    void simulatorLaunchRequested();
    void simulationActiveChanged(bool active);

private slots:
    void startStreaming();
    void stopStreaming();
    void modeSelectionChanged();

private:
    void compilePackets();
    bool ensureCompiled();
    bool hasSelectedMode() const;
    bool simulatorModeActive() const;
    void setRunning(bool running);
    void invalidatePackets();

    QToolButton *m_binButton = nullptr;
    QToolButton *m_simulatorButton = nullptr;
    QToolButton *m_uartButton = nullptr;
    QToolButton *m_startButton = nullptr;
    QToolButton *m_stopButton = nullptr;

    FpgaPacketBundle m_bundle;
    bool m_hasBundle = false;
    bool m_running = false;
};
