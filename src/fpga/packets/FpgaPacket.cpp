//FpgaPacket - один сформированный пакет протокола загрузки ПЛИС

#include "fpga/packets/FpgaPacket.h"

QString FpgaPacket::typeName() const
{
    switch (type) {
    case FpgaPacketType::RotationMemory:
        return QStringLiteral("rotation memory");
    case FpgaPacketType::Parameter:
        return QStringLiteral("parameter");
    case FpgaPacketType::StaticMemory:
        return QStringLiteral("static memory");
    case FpgaPacketType::Layer3d:
        return QStringLiteral("layer 3d");
    }
    return QStringLiteral("unknown");
}

qsizetype FpgaPacketBundle::totalBytes() const
{
    qsizetype total = 0;
    for (const FpgaPacket &packet : packets)
        total += packet.bytes.size();
    return total;
}

