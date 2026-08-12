//FpgaPacketFramer - общий каркас пакетов SAIN/length/type/payload/FCS

#pragma once

#include <QByteArray>

class FpgaPacketFramer
{
public:
    static quint32 fletcher32(const QByteArray &data);
    static QByteArray frame(quint8 type, const QByteArray &payload, quint32 *fcsOut = nullptr);
};

