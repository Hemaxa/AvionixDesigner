//FpgaSimulatorPanel - панель симулятора ПЛИС

#include "FpgaSimulatorPanel.h"
#include "FpgaScreenWidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QImage>

FpgaSimulatorPanel::FpgaSimulatorPanel(QWidget *parent)
    : BasePanel(parent)
{
    setPanelName(QStringLiteral("FpgaSimulatorPanel"));

    m_screenWidget = new FpgaScreenWidget(this);
    m_warningsLabel = new QLabel(this);
    m_warningsLabel->setObjectName(QStringLiteral("FpgaSimulatorWarningsLabel"));
    m_warningsLabel->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    layout->addWidget(m_screenWidget, 1);
    layout->addWidget(m_warningsLabel);
}

void FpgaSimulatorPanel::loadBundle(const FpgaPacketBundle &bundle)
{
    m_simulator.loadBundle(bundle);
    const QImage frame = m_simulator.renderFrame();
    m_screenWidget->setFrameImage(frame);

    const QStringList warnings = m_simulator.warnings();
    if (warnings.isEmpty()) {
        m_warningsLabel->clear();
        m_warningsLabel->hide();
    } else {
        m_warningsLabel->setText(warnings.join(QStringLiteral("\n")));
        m_warningsLabel->show();
    }
}

void FpgaSimulatorPanel::setSimulationActive(bool active)
{
    if (m_screenWidget)
        m_screenWidget->setSimulationActive(active);
}
