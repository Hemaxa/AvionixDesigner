//FpgaStreamingPanel - панель выбора способа загрузки пакетов ПЛИС

#include "FpgaStreamingPanel.h"

#include "fpga/FpgaProjectPacketBuilder.h"
#include "fpga/transport/FpgaPacketDump.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QAbstractItemView>

namespace {
QString fcsText(quint32 value)
{
    return QStringLiteral("0x%1").arg(value, 8, 16, QLatin1Char('0')).toUpper();
}

QString targetKey(FpgaStreamingPanel::Target target)
{
    switch (target) {
    case FpgaStreamingPanel::Target::Bin:
        return QStringLiteral("bin");
    case FpgaStreamingPanel::Target::Simulator:
        return QStringLiteral("simulator");
    case FpgaStreamingPanel::Target::Uart:
        return QStringLiteral("uart");
    }
    return QStringLiteral("bin");
}
}

FpgaStreamingPanel::FpgaStreamingPanel(QWidget *parent)
    : BasePanel(parent)
{
    setPanelName(QStringLiteral("FpgaStreamingPanel"));

    m_targetCombo = new QComboBox(this);
    m_targetCombo->addItem(QStringLiteral("BIN dump"), targetKey(Target::Bin));
    m_targetCombo->addItem(QStringLiteral("Simulator"), targetKey(Target::Simulator));
    m_targetCombo->addItem(QStringLiteral("UART"), targetKey(Target::Uart));

    m_compileButton = new QPushButton(QStringLiteral("Скомпилировать"), this);
    m_runButton = new QPushButton(QStringLiteral("Запустить"), this);
    m_summaryLabel = new QLabel(QStringLiteral("Пакеты не собраны"), this);
    m_warningsLabel = new QLabel(this);
    m_warningsLabel->setWordWrap(true);

    m_packetTable = new QTableWidget(this);
    m_packetTable->setColumnCount(8);
    m_packetTable->setHorizontalHeaderLabels({
        QStringLiteral("#"),
        QStringLiteral("Тип"),
        QStringLiteral("Объект"),
        QStringLiteral("obj_id"),
        QStringLiteral("mem_id"),
        QStringLiteral("Адрес"),
        QStringLiteral("Payload"),
        QStringLiteral("FCS")
    });
    m_packetTable->verticalHeader()->setVisible(false);
    m_packetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_packetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_packetTable->setAlternatingRowColors(true);
    m_packetTable->horizontalHeader()->setStretchLastSection(true);
    m_packetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_targetCombo, 1);
    buttonLayout->addWidget(m_compileButton);
    buttonLayout->addWidget(m_runButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    layout->addLayout(buttonLayout);
    layout->addWidget(m_summaryLabel);
    layout->addWidget(m_warningsLabel);
    layout->addWidget(m_packetTable, 1);

    connect(m_compileButton, &QPushButton::clicked, this, &FpgaStreamingPanel::compilePackets);
    connect(m_runButton, &QPushButton::clicked, this, &FpgaStreamingPanel::runSelectedTarget);
}

FpgaStreamingPanel::Target FpgaStreamingPanel::currentTarget() const
{
    const QString key = m_targetCombo->currentData().toString();
    if (key == QStringLiteral("simulator"))
        return Target::Simulator;
    if (key == QStringLiteral("uart"))
        return Target::Uart;
    return Target::Bin;
}

void FpgaStreamingPanel::compilePackets()
{
    QString error;
    m_bundle = FpgaProjectPacketBuilder::buildCurrentProject(&error);
    m_hasBundle = error.isEmpty() && m_bundle.document.isValid();
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Пакеты ПЛИС"), error);
    }
    refreshSummary();
    refreshTable();
}

void FpgaStreamingPanel::runSelectedTarget()
{
    if (!ensureCompiled())
        return;

    switch (currentTarget()) {
    case Target::Bin: {
        const QString fileName = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Сохранить BIN dump"),
            QStringLiteral("fpga_packets.bin"),
            QStringLiteral("BIN (*.bin);;Все файлы (*.*)")
        );
        if (fileName.isEmpty())
            return;

        QString error;
        if (!FpgaPacketDump::writeBinary(fileName, m_bundle, &error)) {
            QMessageBox::warning(this, QStringLiteral("BIN dump"), error);
            return;
        }
        m_summaryLabel->setText(QStringLiteral("BIN сохранен: %1 пакетов, %2 байт")
            .arg(m_bundle.packets.size())
            .arg(m_bundle.totalBytes()));
        break;
    }
    case Target::Simulator:
        emit simulatorLaunchRequested();
        emit simulatorBundleReady(m_bundle);
        break;
    case Target::Uart:
        QMessageBox::information(
            this,
            QStringLiteral("UART"),
            QStringLiteral("UART backend еще не подключен. Пакеты уже можно проверить через BIN dump или симулятор.")
        );
        break;
    }
}

bool FpgaStreamingPanel::ensureCompiled()
{
    if (!m_hasBundle)
        compilePackets();
    return m_hasBundle;
}

void FpgaStreamingPanel::refreshSummary()
{
    if (!m_hasBundle) {
        m_summaryLabel->setText(QStringLiteral("Пакеты не собраны"));
        m_warningsLabel->clear();
        return;
    }

    m_summaryLabel->setText(QStringLiteral("%1 объектов, %2 пакетов, %3 байт")
        .arg(m_bundle.document.objects.size())
        .arg(m_bundle.packets.size())
        .arg(m_bundle.totalBytes()));
    m_warningsLabel->setText(warningsText());
}

void FpgaStreamingPanel::refreshTable()
{
    m_packetTable->setRowCount(m_hasBundle ? m_bundle.packets.size() : 0);
    if (!m_hasBundle)
        return;

    for (int row = 0; row < m_bundle.packets.size(); ++row) {
        const FpgaPacket &packet = m_bundle.packets[row];
        const QStringList values = {
            QString::number(row),
            packet.typeName(),
            packet.objectType,
            packet.objId >= 0 ? QString::number(packet.objId) : QStringLiteral("-"),
            packet.memId >= 0 ? QString::number(packet.memId) : QStringLiteral("-"),
            packet.address >= 0 ? QString::number(packet.address) : QStringLiteral("-"),
            QString::number(packet.payloadSize),
            fcsText(packet.fcs)
        };

        for (int col = 0; col < values.size(); ++col)
            m_packetTable->setItem(row, col, new QTableWidgetItem(values[col]));
    }
}

QString FpgaStreamingPanel::warningsText() const
{
    if (m_bundle.warnings.isEmpty())
        return QString();

    QStringList visible = m_bundle.warnings.mid(0, 3);
    if (m_bundle.warnings.size() > visible.size())
        visible.append(QStringLiteral("..."));
    return visible.join(QStringLiteral("\n"));
}
