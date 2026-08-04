//FpgaPacketCompiler - компилирует строгий FPGA-документ в UART/BIN пакеты

#pragma once

#include "fpga/model/FpgaCompiledDocument.h"
#include "fpga/packets/FpgaPacket.h"

#include <QByteArray>

class FpgaPacketCompiler
{
public:
    FpgaPacketBundle compile(const FpgaCompiledDocument &document) const;

    static QByteArray parameterPayload(int objId, const QByteArray &data);
    static QByteArray memoryPayload(int memId, int address, const QByteArray &data);
    static QByteArray layer3dPayload(const QByteArray &data);

private:
    QList<QByteArray> encodeRotationTiles(const FpgaCompiledDocument &document,
                                          const FpgaCompiledObject &object,
                                          QStringList *warnings) const;
    QByteArray encodeStaticMaskData(const QString &data, QStringList *warnings) const;
};

