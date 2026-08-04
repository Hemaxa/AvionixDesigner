//FpgaStreamingPanel - панель выбора способа загрузки пакетов ПЛИС

#pragma once

#include "BasePanel.h"
#include "fpga/packets/FpgaPacket.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

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

private slots:
    void compilePackets();
    void runSelectedTarget();

private:
    Target currentTarget() const;
    bool ensureCompiled();
    void refreshSummary();
    void refreshTable();
    QString warningsText() const;

    QComboBox *m_targetCombo = nullptr;
    QPushButton *m_compileButton = nullptr;
    QPushButton *m_runButton = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_warningsLabel = nullptr;
    QTableWidget *m_packetTable = nullptr;

    FpgaPacketBundle m_bundle;
    bool m_hasBundle = false;
};
