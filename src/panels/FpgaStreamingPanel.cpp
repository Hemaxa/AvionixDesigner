//FpgaStreamingPanel - панель выбора способа загрузки пакетов ПЛИС

#include "FpgaStreamingPanel.h"

#include "ProjectManager.h"
#include "fpga/FpgaProjectPacketBuilder.h"
#include "fpga/transport/FpgaPacketDump.h"

#include <QFileDialog>
#include <QBoxLayout>
#include <QFrame>
#include <QIcon>
#include <QMessageBox>
#include <QResizeEvent>
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

bool shouldUseVerticalControls(const QWidget *widget)
{
    return widget && widget->height() > widget->width() * 1.15;
}
}

FpgaStreamingPanel::FpgaStreamingPanel(QWidget *parent)
    : BasePanel(parent)
{
    setPanelName(QStringLiteral("FpgaStreamingPanel"));

    m_binButton = createIconButton(QStringLiteral(":/icons/icons/fpga/bin.svg"), QStringLiteral("BIN dump"), this, true);
    m_simulatorButton = createIconButton(QStringLiteral(":/icons/icons/fpga/simulator.svg"), QStringLiteral("Симулятор ПЛИС"), this, true);
    m_uartButton = createIconButton(QStringLiteral(":/icons/icons/fpga/uart.svg"), QStringLiteral("UART"), this, true);
    m_startButton = createIconButton(QStringLiteral(":/icons/icons/fpga/play.svg"), QStringLiteral("Запустить выбранные режимы"), this);
    m_stopButton = createIconButton(QStringLiteral(":/icons/icons/fpga/stop.svg"), QStringLiteral("Остановить симуляцию"), this);
    m_stopButton->setEnabled(false);
    m_simulatorButton->setChecked(true);

    m_modeLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_modeLayout->setSpacing(6);
    m_modeLayout->setAlignment(Qt::AlignCenter);
    m_modeLayout->addWidget(m_binButton, 0, Qt::AlignCenter);
    m_modeLayout->addWidget(m_simulatorButton, 0, Qt::AlignCenter);
    m_modeLayout->addWidget(m_uartButton, 0, Qt::AlignCenter);

    m_separator = new QFrame(this);
    m_separator->setFrameShape(QFrame::VLine);
    m_separator->setObjectName(QStringLiteral("SettingsSeparator"));

    m_buttonLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_buttonLayout->setSpacing(6);
    m_buttonLayout->setAlignment(Qt::AlignCenter);
    m_buttonLayout->addWidget(m_startButton, 0, Qt::AlignCenter);
    m_buttonLayout->addWidget(m_stopButton, 0, Qt::AlignCenter);

    m_layout = new QBoxLayout(QBoxLayout::LeftToRight, this);
    m_layout->setContentsMargins(10, 8, 10, 8);
    m_layout->setSpacing(12);
    m_layout->setAlignment(Qt::AlignCenter);
    m_layout->addLayout(m_modeLayout);
    m_layout->addWidget(m_separator, 0, Qt::AlignCenter);
    m_layout->addLayout(m_buttonLayout);
    m_layout->addStretch(1);

    connect(m_startButton, &QToolButton::clicked, this, &FpgaStreamingPanel::startStreaming);
    connect(m_stopButton, &QToolButton::clicked, this, &FpgaStreamingPanel::stopStreaming);
    connect(m_binButton, &QToolButton::toggled, this, &FpgaStreamingPanel::modeSelectionChanged);
    connect(m_simulatorButton, &QToolButton::toggled, this, &FpgaStreamingPanel::modeSelectionChanged);
    connect(m_uartButton, &QToolButton::toggled, this, &FpgaStreamingPanel::modeSelectionChanged);
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, this, &FpgaStreamingPanel::invalidatePackets);
    updateAdaptiveLayout();
}

void FpgaStreamingPanel::resizeEvent(QResizeEvent *event)
{
    BasePanel::resizeEvent(event);
    updateAdaptiveLayout();
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
    m_stopButton->setEnabled(m_running);
    emit simulationActiveChanged(simulatorModeActive());
}

void FpgaStreamingPanel::invalidatePackets()
{
    if (!m_hasBundle)
        return;

    m_hasBundle = false;
}

void FpgaStreamingPanel::updateAdaptiveLayout()
{
    if (!m_layout || !m_modeLayout || !m_buttonLayout || !m_separator)
        return;

    const bool vertical = shouldUseVerticalControls(this);
    if (m_verticalLayout == vertical)
        return;

    m_verticalLayout = vertical;
    const QBoxLayout::Direction direction = vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
    m_layout->setDirection(direction);
    m_modeLayout->setDirection(direction);
    m_buttonLayout->setDirection(direction);
    m_separator->setFrameShape(vertical ? QFrame::HLine : QFrame::VLine);
    m_layout->setAlignment(Qt::AlignCenter);
    m_modeLayout->setAlignment(Qt::AlignCenter);
    m_buttonLayout->setAlignment(Qt::AlignCenter);
    updateGeometry();
}
