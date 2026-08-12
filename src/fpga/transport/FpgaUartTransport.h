//FpgaUartTransport - отправка сформированных пакетов ПЛИС через UART

#pragma once

#include "fpga/packets/FpgaPacket.h"

#include <QString>

struct FpgaUartConfig
{
    QString portName;
    qint32 baudRate = 921600;
    bool writeInitialZero = true;
    int writeTimeoutMs = 5000;
};

class FpgaUartTransport
{
public:
    static bool writeBundle(const FpgaPacketBundle &bundle, const FpgaUartConfig &config, QString *errorMessage = nullptr);
};
