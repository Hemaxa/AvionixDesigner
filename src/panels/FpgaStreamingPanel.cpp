//FpgaStreamingPanel - панель выбора способа загрузки пакетов ПЛИС

#include "FpgaStreamingPanel.h"

#include "ProjectManager.h"
#include "fpga/FpgaProjectPacketBuilder.h"
#include "fpga/transport/FpgaPacketDump.h"

#include <QFileDialog>
#include <QBoxLayout>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QSerialPortInfo>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QSize>
#include <QSizePolicy>

namespace {
constexpr int kDefaultUartBaudRate = 115200;
constexpr int kDefaultUartIntervalMs = 2000;
constexpr int kMinimumUartIntervalMs = 1000;
constexpr int kMaximumUartIntervalMs = 60000;

QString toolTipWithShortcut(const QString &text, const QKeySequence &shortcut = {})
{
    if (shortcut.isEmpty())
        return text;
    return QStringLiteral("%1 (%2)").arg(text, shortcut.toString(QKeySequence::NativeText));
}

QToolButton* createIconButton(
    const QString &iconPath,
    const QString &toolTip,
    QWidget *parent,
    bool checkable = false,
    const QKeySequence &shortcut = {}
)
{
    auto *button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("SettingsToolButton"));
    button->setIcon(QIcon(iconPath));
    button->setToolTip(toolTipWithShortcut(toolTip, shortcut));
    button->setCheckable(checkable);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}

bool shouldUseVerticalControls(const QWidget *widget)
{
    return widget && widget->height() > widget->width() * 1.15;
}

QStringList availablePortNames()
{
    QStringList names;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    names.reserve(ports.size());
    for (const QSerialPortInfo &port : ports)
        names.append(port.portName());
    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);
    return names;
}

class UartStreamingDialog : public QDialog
{
public:
    explicit UartStreamingDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setObjectName(QStringLiteral("UartStreamingDialog"));
        setWindowTitle(QStringLiteral("UART трансляция"));
        setModal(true);
        setMinimumWidth(430);

        auto *titleLabel = new QLabel(QStringLiteral("UART трансляция"), this);
        titleLabel->setObjectName(QStringLiteral("DialogTitleLabel"));

        auto *descriptionLabel = new QLabel(
            QStringLiteral("Выберите порт, скорость и интервал автоотправки текущих пакетов на ПЛИС.")
        , this);
        descriptionLabel->setObjectName(QStringLiteral("DialogDescriptionLabel"));
        descriptionLabel->setWordWrap(true);

        m_portCombo = new QComboBox(this);
        m_portCombo->setObjectName(QStringLiteral("UartPortCombo"));
        m_portCombo->setEditable(true);
        m_portCombo->setFixedHeight(32);

        auto *refreshButton = new QPushButton(QStringLiteral("Обновить"), this);
        refreshButton->setObjectName(QStringLiteral("SecondaryButton"));
        refreshButton->setFixedHeight(32);
        connect(refreshButton, &QPushButton::clicked, this, [this]() {
            refreshPorts(m_portCombo->currentText());
        });

        auto *portRow = new QWidget(this);
        auto *portLayout = new QHBoxLayout(portRow);
        portLayout->setContentsMargins(0, 0, 0, 0);
        portLayout->setSpacing(8);
        portLayout->addWidget(m_portCombo, 1);
        portLayout->addWidget(refreshButton, 0);

        m_baudRateSpin = new QSpinBox(this);
        m_baudRateSpin->setObjectName(QStringLiteral("UartBaudRateSpin"));
        m_baudRateSpin->setRange(1200, 4000000);
        m_baudRateSpin->setSingleStep(9600);
        m_baudRateSpin->setFixedHeight(32);

        m_intervalSpin = new QSpinBox(this);
        m_intervalSpin->setObjectName(QStringLiteral("UartIntervalSpin"));
        m_intervalSpin->setRange(kMinimumUartIntervalMs, kMaximumUartIntervalMs);
        m_intervalSpin->setSingleStep(250);
        m_intervalSpin->setSuffix(QStringLiteral(" мс"));
        m_intervalSpin->setFixedHeight(32);

        auto *form = new QGridLayout();
        form->setObjectName(QStringLiteral("UartStreamingForm"));
        form->setContentsMargins(0, 0, 0, 0);
        form->setHorizontalSpacing(14);
        form->setVerticalSpacing(10);
        addFormRow(form, 0, QStringLiteral("Порт"), portRow);
        addFormRow(form, 1, QStringLiteral("Скорость"), m_baudRateSpin);
        addFormRow(form, 2, QStringLiteral("Интервал"), m_intervalSpin);
        form->setColumnStretch(1, 1);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Запустить"));
        buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(22, 20, 22, 18);
        layout->setSpacing(14);
        layout->addWidget(titleLabel);
        layout->addWidget(descriptionLabel);
        layout->addSpacing(2);
        layout->addLayout(form);
        layout->addWidget(buttons);
    }

    void loadSettings(const QSettings &settings)
    {
        const QString savedPort = settings.value(QStringLiteral("fpga/uartPort")).toString();
        refreshPorts(savedPort);
        m_baudRateSpin->setValue(settings.value(QStringLiteral("fpga/uartBaudRate"), kDefaultUartBaudRate).toInt());
        m_intervalSpin->setValue(settings.value(QStringLiteral("fpga/uartIntervalMs"), kDefaultUartIntervalMs).toInt());
    }

    QString portName() const { return m_portCombo->currentText().trimmed(); }
    int baudRate() const { return m_baudRateSpin->value(); }
    int intervalMs() const { return m_intervalSpin->value(); }

private:
    void addFormRow(QGridLayout *form, int row, const QString &labelText, QWidget *field)
    {
        auto *label = new QLabel(labelText, this);
        label->setObjectName(QStringLiteral("DialogFieldLabel"));
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setMinimumWidth(82);
        form->addWidget(label, row, 0, Qt::AlignRight | Qt::AlignVCenter);
        form->addWidget(field, row, 1);
    }

    void refreshPorts(const QString &preferredPort)
    {
        const QString current = preferredPort.trimmed().isEmpty() ? m_portCombo->currentText().trimmed() : preferredPort.trimmed();
        QStringList ports = availablePortNames();
        if (!current.isEmpty() && !ports.contains(current))
            ports.prepend(current);

        m_portCombo->clear();
        m_portCombo->addItems(ports);
        if (!current.isEmpty())
            m_portCombo->setCurrentText(current);
    }

    QComboBox *m_portCombo = nullptr;
    QSpinBox *m_baudRateSpin = nullptr;
    QSpinBox *m_intervalSpin = nullptr;
};
}

FpgaStreamingPanel::FpgaStreamingPanel(QWidget *parent)
    : BasePanel(parent)
{
    setPanelName(QStringLiteral("FpgaStreamingPanel"));

    m_binButton = createIconButton(QStringLiteral(":/icons/icons/fpga/bin.svg"), QStringLiteral("BIN dump"), this, true, QKeySequence(QStringLiteral("Ctrl+Alt+B")));
    m_simulatorButton = createIconButton(QStringLiteral(":/icons/icons/fpga/simulator.svg"), QStringLiteral("Симулятор ПЛИС"), this, true, QKeySequence(QStringLiteral("Ctrl+Alt+F")));
    m_uartButton = createIconButton(QStringLiteral(":/icons/icons/fpga/uart.svg"), QStringLiteral("UART"), this, true, QKeySequence(QStringLiteral("Ctrl+Alt+U")));
    m_startButton = createIconButton(QStringLiteral(":/icons/icons/fpga/play.svg"), QStringLiteral("Запустить выбранные режимы"), this, false, QKeySequence(QStringLiteral("Ctrl+Enter")));
    m_stopButton = createIconButton(QStringLiteral(":/icons/icons/fpga/stop.svg"), QStringLiteral("Остановить активные режимы"), this, false, QKeySequence(QStringLiteral("Ctrl+Shift+Enter")));
    m_stopButton->setEnabled(false);
    m_simulatorButton->setChecked(true);

    m_uartTimer = new QTimer(this);
    m_uartTimer->setTimerType(Qt::CoarseTimer);

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
    connect(m_uartTimer, &QTimer::timeout, this, &FpgaStreamingPanel::transmitUartFrame);
    updateAdaptiveLayout();
}

void FpgaStreamingPanel::resizeEvent(QResizeEvent *event)
{
    BasePanel::resizeEvent(event);
    updateAdaptiveLayout();
}

bool FpgaStreamingPanel::compilePackets(bool showError)
{
    QString error;
    m_bundle = FpgaProjectPacketBuilder::buildCurrentProject(&error);
    m_hasBundle = error.isEmpty() && m_bundle.document.isValid();
    if (!error.isEmpty() && showError) {
        QMessageBox::warning(this, QStringLiteral("Пакеты ПЛИС"), error);
    }
    return m_hasBundle;
}

void FpgaStreamingPanel::startStreaming()
{
    if (!hasSelectedMode()) {
        QMessageBox::information(this, QStringLiteral("ПЛИС"), QStringLiteral("Выберите хотя бы один режим загрузки."));
        return;
    }

    if (!ensureCompiled())
        return;

    if (m_uartButton->isChecked()) {
        if (!configureUartStreaming())
            return;
    }

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
        startUartStreaming();
    }

    setRunning(m_simulatorButton->isChecked() || m_uartStreaming);
}

void FpgaStreamingPanel::stopStreaming()
{
    stopUartStreaming();
    setRunning(false);
}

void FpgaStreamingPanel::toggleBinMode()
{
    m_binButton->setChecked(!m_binButton->isChecked());
}

void FpgaStreamingPanel::toggleSimulatorMode()
{
    m_simulatorButton->setChecked(!m_simulatorButton->isChecked());
}

void FpgaStreamingPanel::toggleUartMode()
{
    m_uartButton->setChecked(!m_uartButton->isChecked());
}

void FpgaStreamingPanel::modeSelectionChanged()
{
    if (m_running)
        emit simulationActiveChanged(simulatorModeActive());
}

bool FpgaStreamingPanel::ensureCompiled()
{
    if (!m_hasBundle)
        return compilePackets();
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

bool FpgaStreamingPanel::configureUartStreaming()
{
    QSettings settings(QStringLiteral("Avionix"), QStringLiteral("Designer"));
    UartStreamingDialog dialog(this);
    dialog.loadSettings(settings);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    if (dialog.portName().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("UART"), QStringLiteral("Выберите или введите UART порт."));
        return false;
    }

    m_uartConfig.portName = dialog.portName();
    m_uartConfig.baudRate = dialog.baudRate();
    m_uartConfig.writeInitialZero = true;
    m_uartConfig.writeTimeoutMs = qMin(1000, qMax(250, dialog.intervalMs() / 2));
    m_uartIntervalMs = dialog.intervalMs();

    settings.setValue(QStringLiteral("fpga/uartPort"), m_uartConfig.portName);
    settings.setValue(QStringLiteral("fpga/uartBaudRate"), m_uartConfig.baudRate);
    settings.setValue(QStringLiteral("fpga/uartIntervalMs"), m_uartIntervalMs);
    return true;
}

bool FpgaStreamingPanel::sendCurrentBundleViaUart(bool showSuccessMessage)
{
    if (m_uartSending)
        return true;

    QString error;
    m_uartSending = true;
    const bool success = FpgaUartTransport::writeBundle(m_bundle, m_uartConfig, &error);
    m_uartSending = false;

    if (!success) {
        QMessageBox::warning(this, QStringLiteral("UART"), error);
        return false;
    }

    if (showSuccessMessage) {
        QMessageBox::information(
            this,
            QStringLiteral("UART"),
            QStringLiteral("Трансляция запущена: %1 пакетов, %2 байт каждые %3 мс")
                .arg(m_bundle.packets.size())
                .arg(m_bundle.totalBytes() + 1)
                .arg(m_uartIntervalMs)
        );
    }
    return true;
}

void FpgaStreamingPanel::startUartStreaming()
{
    m_uartStreaming = true;
    if (!sendCurrentBundleViaUart(false)) {
        stopUartStreaming();
        return;
    }
    m_uartTimer->start(m_uartIntervalMs);
}

void FpgaStreamingPanel::stopUartStreaming()
{
    m_uartStreaming = false;
    if (m_uartTimer)
        m_uartTimer->stop();
}

void FpgaStreamingPanel::transmitUartFrame()
{
    if (!m_uartStreaming || m_uartConfig.portName.isEmpty())
        return;

    if (!compilePackets(false)) {
        stopUartStreaming();
        QMessageBox::warning(this, QStringLiteral("UART"), QStringLiteral("Трансляция остановлена: не удалось сформировать пакеты."));
        setRunning(simulatorModeActive());
        return;
    }

    if (!sendCurrentBundleViaUart(false)) {
        stopUartStreaming();
        setRunning(simulatorModeActive());
    }
}

void FpgaStreamingPanel::setRunning(bool running)
{
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
