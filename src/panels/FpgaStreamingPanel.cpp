//FpgaStreamingPanel - панель выбора способа загрузки пакетов ПЛИС

#include "FpgaStreamingPanel.h"

#include "ProjectManager.h"
#include "FlowLayout.h"
#include "ThreeDViewObject.h"
#include "fpga/packets/FpgaPacketCompiler.h"
#include "fpga/FpgaProjectPacketBuilder.h"
#include "fpga/transport/FpgaPacketDump.h"

#include <QFileDialog>
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
#include <QSerialPortInfo>
#include "ApplicationSettings.h"
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QSize>
#include <QSizePolicy>

namespace {
constexpr int kDefaultUartBaudRate = 115200;
constexpr int kDefaultUartIntervalMs = 20;
constexpr int kMinimumUartIntervalMs = 0;
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

// Параметры порта, двух скоростей и интервала перед началом UART-сеанса.
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
            QStringLiteral("Сначала загружается сцена, затем передаются положения самолета. Интервал 0 — без дополнительной паузы.")
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

        m_loadBaudRateSpin = new QSpinBox(this);
        m_loadBaudRateSpin->setObjectName(QStringLiteral("UartLoadBaudRateSpin"));
        m_loadBaudRateSpin->setRange(1200, 4000000);
        m_loadBaudRateSpin->setSingleStep(9600);
        m_loadBaudRateSpin->setFixedHeight(32);

        m_baudRateSpin = new QSpinBox(this);
        m_baudRateSpin->setObjectName(QStringLiteral("UartBaudRateSpin"));
        m_baudRateSpin->setRange(1200, 4000000);
        m_baudRateSpin->setSingleStep(9600);
        m_baudRateSpin->setFixedHeight(32);

        m_intervalSpin = new QSpinBox(this);
        m_intervalSpin->setObjectName(QStringLiteral("UartIntervalSpin"));
        m_intervalSpin->setRange(kMinimumUartIntervalMs, kMaximumUartIntervalMs);
        m_intervalSpin->setSingleStep(1);
        m_intervalSpin->setSpecialValueText(QStringLiteral("Без дополнительной паузы"));
        m_intervalSpin->setSuffix(QStringLiteral(" мс"));
        m_intervalSpin->setFixedHeight(32);

        auto *form = new QGridLayout();
        form->setObjectName(QStringLiteral("UartStreamingForm"));
        form->setContentsMargins(0, 0, 0, 0);
        form->setHorizontalSpacing(14);
        form->setVerticalSpacing(10);
        addFormRow(form, 0, QStringLiteral("Порт"), portRow);
        addFormRow(form, 1, QStringLiteral("Загрузка, бод"), m_loadBaudRateSpin);
        addFormRow(form, 2, QStringLiteral("Управление, бод"), m_baudRateSpin);
        addFormRow(form, 3, QStringLiteral("Интервал"), m_intervalSpin);
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
        m_loadBaudRateSpin->setValue(settings.value(QStringLiteral("fpga/uartLoadBaudRate"), 921600).toInt());
        // Старая общая скорость могла относиться к загрузке сцены, поэтому
        // не используем ее для нового отдельного параметра скорости управления.
        m_baudRateSpin->setValue(settings.value(QStringLiteral("fpga/uartControlBaudRate"), kDefaultUartBaudRate).toInt());
        m_intervalSpin->setValue(settings.value(QStringLiteral("fpga/uartIntervalMs"), kDefaultUartIntervalMs).toInt());
    }

    QString portName() const { return m_portCombo->currentText().trimmed(); }
    int loadBaudRate() const { return m_loadBaudRateSpin->value(); }
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
    QSpinBox *m_loadBaudRateSpin = nullptr;
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

    m_uartTransport = new FpgaUartTransport(this);
    connect(m_uartTransport, &FpgaUartTransport::setupComplete, this, [this]() {
        emit ProjectManager::instance()->logMessage(QStringLiteral("UART: сцена загружена; управление %1 бод, интервал %2 мс.")
            .arg(m_uartConfig.baudRate).arg(m_uartConfig.intervalMs));
    });
    connect(m_uartTransport, &FpgaUartTransport::positionRequested, this, &FpgaStreamingPanel::transmitUartFrame);
    connect(m_uartTransport, &FpgaUartTransport::failed, this, [this](const QString &error) {
        stopUartStreaming();
        setRunning(simulatorModeActive());
        QMessageBox::warning(this, QStringLiteral("UART"), error);
    });

    auto *flow = createFlowPanel(this);
    for (auto *button : {m_binButton, m_simulatorButton, m_uartButton, m_startButton, m_stopButton})
        flow->addWidget(button);

    connect(m_startButton, &QToolButton::clicked, this, &FpgaStreamingPanel::startStreaming);
    connect(m_stopButton, &QToolButton::clicked, this, &FpgaStreamingPanel::stopStreaming);
    connect(m_binButton, &QToolButton::toggled, this, &FpgaStreamingPanel::modeSelectionChanged);
    connect(m_simulatorButton, &QToolButton::toggled, this, &FpgaStreamingPanel::modeSelectionChanged);
    connect(m_uartButton, &QToolButton::toggled, this, &FpgaStreamingPanel::modeSelectionChanged);
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, this, &FpgaStreamingPanel::invalidatePackets);
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, &FpgaStreamingPanel::invalidatePackets);
}

FpgaStreamingPanel::~FpgaStreamingPanel() { stopUartStreaming(); }

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
    auto settings = ApplicationSettings::create();
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
    m_uartConfig.loadBaudRate = dialog.loadBaudRate();
    m_uartConfig.intervalMs = dialog.intervalMs();
    m_uartConfig.writeInitialZero = true;
    m_uartConfig.writeTimeoutMs = 5000;

    settings.setValue(QStringLiteral("fpga/uartPort"), m_uartConfig.portName);
    settings.setValue(QStringLiteral("fpga/uartLoadBaudRate"), m_uartConfig.loadBaudRate);
    settings.setValue(QStringLiteral("fpga/uartControlBaudRate"), m_uartConfig.baudRate);
    settings.setValue(QStringLiteral("fpga/uartIntervalMs"), m_uartConfig.intervalMs);
    return true;
}

void FpgaStreamingPanel::startUartStreaming()
{
    m_lastSceneRevision = ProjectManager::instance()->sceneRevision();
    QString error;
    m_uartStreaming = m_uartTransport->startStreaming(m_bundle, m_uartConfig, &error);
    if (!m_uartStreaming) QMessageBox::warning(this, QStringLiteral("UART"), error);
}

void FpgaStreamingPanel::stopUartStreaming()
{
    m_uartStreaming = false;
    if (m_uartTransport) m_uartTransport->stopStreaming();
}

void FpgaStreamingPanel::transmitUartFrame()
{
    m_updateScheduled = false;
    if (!m_uartStreaming) return;
    auto *project = ProjectManager::instance();
    if (project->sceneRevision() != m_lastSceneRevision) {
        if (!compilePackets(false)) {
            stopUartStreaming();
            setRunning(simulatorModeActive());
            QMessageBox::warning(this, QStringLiteral("UART"), QStringLiteral("Не удалось сформировать сцену."));
            return;
        }
        m_uartTransport->updateScene(m_bundle);
        m_lastSceneRevision = project->sceneRevision();
    }
    auto *view = project->threeDViewObject();
    m_uartTransport->updatePosition(view && view->isExportEnabled()
        ? FpgaPacketCompiler::layer3dPositionPacket(view->serializeInit()).bytes : QByteArray());
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
    m_hasBundle = false;
    if (m_uartStreaming && !m_updateScheduled
        && ProjectManager::instance()->sceneRevision() != m_lastSceneRevision) {
        m_updateScheduled = true;
        QTimer::singleShot(0, this, &FpgaStreamingPanel::transmitUartFrame);
    }
}
