//FpgaPacketFramer - общий каркас пакетов SAIN/length/type/payload/FCS

#include "fpga/packets/FpgaPacketFramer.h"

namespace {
void appendUint16(QByteArray *data, quint16 value)
{
    data->append(static_cast<char>((value >> 8) & 0xFF));
    data->append(static_cast<char>(value & 0xFF));
}

void appendUint32(QByteArray *data, quint32 value)
{
    data->append(static_cast<char>((value >> 24) & 0xFF));
    data->append(static_cast<char>((value >> 16) & 0xFF));
    data->append(static_cast<char>((value >> 8) & 0xFF));
    data->append(static_cast<char>(value & 0xFF));
}
}

quint32 FpgaPacketFramer::fletcher32(const QByteArray &data)
{
    quint32 a = 0;
    quint32 b = 0;
    for (const char rawByte : data) {
        const quint8 byte = static_cast<quint8>(rawByte);
        a = (a + byte) & 0xFFFF;
        b = (a + b) & 0xFFFF;
    }
    return (b << 16) | a;
}

QByteArray FpgaPacketFramer::frame(quint8 type, const QByteArray &payload, quint32 *fcsOut)
{
    QByteArray packet;
    packet.reserve(4 + 2 + 1 + payload.size() + 4);
    packet.append(char(0x53));
    packet.append(char(0x41));
    packet.append(char(0x49));
    packet.append(char(0x4E));
    packet.append(static_cast<char>(type));
    packet.append(payload);

    const quint16 length = static_cast<quint16>(packet.size() - 4 + 4);
    QByteArray lengthBytes;
    appendUint16(&lengthBytes, length);
    packet.insert(4, lengthBytes);

    const quint32 fcs = fletcher32(packet);
    appendUint32(&packet, fcs);
    if (fcsOut)
        *fcsOut = fcs;

    return packet;
}

