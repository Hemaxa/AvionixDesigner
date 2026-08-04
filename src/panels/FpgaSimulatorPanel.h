//FpgaSimulatorPanel - панель симулятора ПЛИС

#pragma once

#include "BasePanel.h"
#include "fpga/simulator/FpgaSimulator.h"
#include "fpga/packets/FpgaPacket.h"

class FpgaScreenWidget;
class QLabel;

class FpgaSimulatorPanel : public BasePanel
{
    Q_OBJECT

public:
    explicit FpgaSimulatorPanel(QWidget *parent = nullptr);

public slots:
    void loadBundle(const FpgaPacketBundle &bundle);

private:
    FpgaSimulator m_simulator;
    FpgaScreenWidget *m_screenWidget = nullptr;
    QLabel *m_warningsLabel = nullptr;
};
