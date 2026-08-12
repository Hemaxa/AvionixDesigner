//FpgaPacket - один сформированный пакет протокола загрузки ПЛИС

#pragma once

#include "fpga/model/FpgaCompiledDocument.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

enum class FpgaPacketType : quint8
{
    RotationMemory = 0x02,
    Parameter = 0x03,
    StaticMemory = 0x04,
    Layer3d = 0x06
};

struct FpgaPacket
{
    FpgaPacketType type = FpgaPacketType::Parameter;
    QByteArray bytes;
    QString objectType;
    int objectIndex = -1;
    int parameterIndex = -1;
    int objId = -1;
    int memId = -1;
    int address = -1;
    int payloadSize = 0;
    quint32 fcs = 0;

    QString typeName() const;
};

struct FpgaPacketBundle
{
    FpgaCompiledDocument document;
    QList<FpgaPacket> packets;
    QStringList warnings;

    qsizetype totalBytes() const;
};

