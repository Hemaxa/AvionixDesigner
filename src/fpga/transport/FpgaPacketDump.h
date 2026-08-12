//FpgaPacketDump - запись сформированных пакетов в бинарный файл для отладки

#pragma once

#include "fpga/packets/FpgaPacket.h"

#include <QString>

class FpgaPacketDump
{
public:
    static bool writeBinary(const QString &fileName, const FpgaPacketBundle &bundle, QString *errorMessage = nullptr);
};

