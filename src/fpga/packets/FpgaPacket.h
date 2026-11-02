// FpgaPacket - один сформированный пакет протокола загрузки ПЛИС.

#pragma once

#include "fpga/model/FpgaHardwareScene.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// FpgaPacketType - коды команд существующего UART-протокола ПЛИС.
enum class FpgaPacketType : quint8
{
    RotationMemory = 0x02,
    Parameter = 0x03,
    StaticMemory = 0x04,
    Layer3d = 0x06
};

// FpgaPacket - один сформированный пакет протокола загрузки ПЛИС.
struct FpgaPacket
{
    FpgaPacketType type = FpgaPacketType::Parameter;
    QByteArray bytes; // Полный кадр SAIN: заголовок, payload и контрольная сумма.
    // Сопроводительные сведения ниже нужны интерфейсу/симулятору и не входят в payload.
    QString objectType;
    int objectIndex = -1;
    int parameterIndex = -1;
    int objId = -1;
    int memId = -1;
    int address = -1;
    int payloadSize = 0; // Размер данных слова/блока, без его адреса и длины.
    quint32 fcs = 0;

    QString typeName() const;
};

// FpgaPacketBundle - аппаратная сцена, сформированные пакеты и предупреждения.
struct FpgaPacketBundle
{
    FpgaHardwareScene document;
    QList<FpgaPacket> packets;
    QStringList warnings;

    qsizetype totalBytes() const;
};

