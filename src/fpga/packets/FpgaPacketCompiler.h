//FpgaPacketCompiler - упаковывает готовую аппаратную сцену в UART/BIN пакеты.

#pragma once

#include "fpga/model/FpgaHardwareScene.h"
#include "fpga/packets/FpgaPacket.h"

#include <QByteArray>

class FpgaPacketCompiler
{
public:
    FpgaPacketBundle compile(const FpgaHardwareScene &document, QString *errorMessage = nullptr) const;

    static QByteArray parameterPayload(int objId, const QByteArray &data);
    static QByteArray memoryPayload(int memId, int address, const QByteArray &data);
    static QByteArray layer3dPayload(const QByteArray &data);
    static FpgaPacket layer3dSidePacket(bool left);
    static FpgaPacket layer3dPositionPacket(const QString &initHex);

private:
    QList<QByteArray> encodeRotationTiles(const FpgaHardwareScene &document,
                                          const FpgaHardwareObject &object,
                                          QStringList *warnings) const;
    QByteArray encodeStaticMaskData(const QString &data, QStringList *warnings) const;
};

