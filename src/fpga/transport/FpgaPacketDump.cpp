//FpgaPacketDump - запись сформированных пакетов в бинарный файл для отладки

#include "fpga/transport/FpgaPacketDump.h"

#include <QFile>

bool FpgaPacketDump::writeBinary(const QString &fileName, const FpgaPacketBundle &bundle, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    for (const FpgaPacket &packet : bundle.packets) {
        if (file.write(packet.bytes) != packet.bytes.size()) {
            if (errorMessage)
                *errorMessage = file.errorString();
            return false;
        }
    }

    return true;
}

