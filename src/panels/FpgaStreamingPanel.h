//FpgaStreamingPanel - панель выбора способа загрузки пакетов ПЛИС

#pragma once

#include "BasePanel.h"
#include "fpga/packets/FpgaPacket.h"

class QBoxLayout;
class QFrame;
class QResizeEvent;
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

private slots:
    void startStreaming();
    void stopStreaming();
    void modeSelectionChanged();

private:
    void resizeEvent(QResizeEvent *event) override;
    void compilePackets();
    bool ensureCompiled();
    bool hasSelectedMode() const;
    bool simulatorModeActive() const;
    bool sendViaUart();
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
    bool m_hasBundle = false;
    bool m_running = false;
    bool m_verticalLayout = false;
};
