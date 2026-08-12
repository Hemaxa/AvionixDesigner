//FpgaUartTransport - отправка сформированных пакетов ПЛИС через UART

#include "fpga/transport/FpgaUartTransport.h"

#include <QSerialPort>

namespace {
QByteArray bundleToUartBytes(const FpgaPacketBundle &bundle, bool writeInitialZero)
{
    QByteArray data;
    data.reserve(static_cast<qsizetype>(bundle.totalBytes()) + (writeInitialZero ? 1 : 0));
    if (writeInitialZero)
        data.append(char(0x00));
    for (const FpgaPacket &packet : bundle.packets)
        data.append(packet.bytes);
    return data;
}
}

bool FpgaUartTransport::writeBundle(const FpgaPacketBundle &bundle, const FpgaUartConfig &config, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    if (config.portName.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Не выбран UART порт.");
        return false;
    }

    QSerialPort port;
    port.setPortName(config.portName.trimmed());
    port.setBaudRate(config.baudRate);
    port.setDataBits(QSerialPort::Data8);
    port.setParity(QSerialPort::NoParity);
    port.setStopBits(QSerialPort::OneStop);
    port.setFlowControl(QSerialPort::NoFlowControl);

    if (!port.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Не удалось открыть UART порт %1: %2")
                .arg(config.portName, port.errorString());
        return false;
    }

    const QByteArray data = bundleToUartBytes(bundle, config.writeInitialZero);
    qsizetype offset = 0;
    while (offset < data.size()) {
        const qint64 written = port.write(data.constData() + offset, data.size() - offset);
        if (written < 0) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Ошибка записи в UART порт %1: %2")
                    .arg(config.portName, port.errorString());
            return false;
        }
        offset += written;
        if (!port.waitForBytesWritten(config.writeTimeoutMs)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Тайм-аут записи в UART порт %1: %2")
                    .arg(config.portName, port.errorString());
            return false;
        }
    }

    port.close();
    return true;
}
