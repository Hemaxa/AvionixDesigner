//FpgaStreamingPanel - панель выбора способа загрузки пакетов ПЛИС

#include "FpgaStreamingPanel.h"

#include "ProjectManager.h"
#include "fpga/FpgaProjectPacketBuilder.h"
#include "fpga/transport/FpgaPacketDump.h"

#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QToolButton>
#include <QSize>
#include <QSizePolicy>

namespace {
QToolButton* createIconButton(const QString &iconPath, const QString &toolTip, QWidget *parent, bool checkable = false)
{
    auto *button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("SettingsToolButton"));
    button->setIcon(QIcon(iconPath));
    button->setToolTip(toolTip);
    button->setCheckable(checkable);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}
}

FpgaStreamingPanel::FpgaStreamingPanel(QWidget *parent)
    : BasePanel(parent)
{
    setPanelName(QStringLiteral("FpgaStreamingPanel"));

    m_binButton = createIconButton(QStringLiteral(":/icons/icons/fpga/bin.svg"), QStringLiteral("BIN dump"), this, true);
    m_simulatorButton = createIconButton(QStringLiteral(":/icons/icons/fpga/simulator.svg"), QStringLiteral("Симулятор ПЛИС"), this, true);
    m_uartButton = createIconButton(QStringLiteral(":/icons/icons/fpga/uart.svg"), QStringLiteral("UART"), this, true);
    m_compileButton = createIconButton(QStringLiteral(":/icons/icons/fpga/compile.svg"), QStringLiteral("Скомпилировать пакеты"), this);
    m_startButton = createIconButton(QStringLiteral(":/icons/icons/fpga/play.svg"), QStringLiteral("Запустить выбранные режимы"), this);
    m_stopButton = createIconButton(QStringLiteral(":/icons/icons/fpga/stop.svg"), QStringLiteral("Остановить симуляцию"), this);
    m_stopButton->setEnabled(false);
    m_simulatorButton->setChecked(true);

    auto *modeLayout = new QHBoxLayout();
    modeLayout->setSpacing(6);
    modeLayout->setAlignment(Qt::AlignVCenter);
    modeLayout->addWidget(m_binButton);
    modeLayout->addWidget(m_simulatorButton);
    modeLayout->addWidget(m_uartButton);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setObjectName(QStringLiteral("SettingsSeparator"));

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(6);
    buttonLayout->setAlignment(Qt::AlignVCenter);
    buttonLayout->addWidget(m_compileButton);
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignVCenter);
    layout->addLayout(modeLayout);
    layout->addWidget(separator, 0, Qt::AlignVCenter);
    layout->addLayout(buttonLayout);
    layout->addStretch(1);

    connect(m_compileButton, &QToolButton::clicked, this, &FpgaStreamingPanel::compilePackets);
    connect(m_startButton, &QToolButton::clicked, this, &FpgaStreamingPanel::startStreaming);
    connect(m_stopButton, &QToolButton::clicked, this, &FpgaStreamingPanel::stopStreaming);
    connect(m_binButton, &QToolButton::toggled, this, &FpgaStreamingPanel::modeSelectionChanged);
    connect(m_simulatorButton, &QToolButton::toggled, this, &FpgaStreamingPanel::modeSelectionChanged);
    connect(m_uartButton, &QToolButton::toggled, this, &FpgaStreamingPanel::modeSelectionChanged);
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, this, &FpgaStreamingPanel::invalidatePackets);
}

void FpgaStreamingPanel::compilePackets()
{
    QString error;
    m_bundle = FpgaProjectPacketBuilder::buildCurrentProject(&error);
    m_hasBundle = error.isEmpty() && m_bundle.document.isValid();
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Пакеты ПЛИС"), error);
    }
}

void FpgaStreamingPanel::startStreaming()
{
    if (!hasSelectedMode()) {
        QMessageBox::information(this, QStringLiteral("ПЛИС"), QStringLiteral("Выберите хотя бы один режим загрузки."));
        return;
    }

    if (!ensureCompiled())
        return;

    if (m_binButton->isChecked()) {
        const QString fileName = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Сохранить BIN dump"),
            QStringLiteral("fpga_packets.bin"),
            QStringLiteral("BIN (*.bin);;Все файлы (*.*)")
        );
        if (!fileName.isEmpty()) {
            QString error;
            if (!FpgaPacketDump::writeBinary(fileName, m_bundle, &error)) {
                QMessageBox::warning(this, QStringLiteral("BIN dump"), error);
            } else {
                QMessageBox::information(
                    this,
                    QStringLiteral("BIN dump"),
                    QStringLiteral("BIN сохранен: %1 пакетов, %2 байт")
                        .arg(m_bundle.packets.size())
                        .arg(m_bundle.totalBytes())
                );
            }
        }
    }

    if (m_simulatorButton->isChecked()) {
        emit simulatorLaunchRequested();
        emit simulatorBundleReady(m_bundle);
    }

    if (m_uartButton->isChecked()) {
        QMessageBox::information(
            this,
            QStringLiteral("UART"),
            QStringLiteral("UART backend еще не подключен. Пакеты уже можно проверить через BIN dump или симулятор.")
        );
    }

    setRunning(m_simulatorButton->isChecked());
}

void FpgaStreamingPanel::stopStreaming()
{
    setRunning(false);
}

void FpgaStreamingPanel::modeSelectionChanged()
{
    if (m_running)
        emit simulationActiveChanged(simulatorModeActive());
}

bool FpgaStreamingPanel::ensureCompiled()
{
    if (!m_hasBundle)
        compilePackets();
    return m_hasBundle;
}

bool FpgaStreamingPanel::hasSelectedMode() const
{
    return m_binButton->isChecked() || m_simulatorButton->isChecked() || m_uartButton->isChecked();
}

bool FpgaStreamingPanel::simulatorModeActive() const
{
    return m_running && m_simulatorButton->isChecked();
}

void FpgaStreamingPanel::setRunning(bool running)
{
    if (m_running == running) {
        emit simulationActiveChanged(simulatorModeActive());
        return;
    }

    m_running = running;
    m_startButton->setEnabled(!m_running);
    m_stopButton->setEnabled(m_running);
    emit simulationActiveChanged(simulatorModeActive());
}

void FpgaStreamingPanel::invalidatePackets()
{
    if (!m_hasBundle)
        return;

    m_hasBundle = false;
}
