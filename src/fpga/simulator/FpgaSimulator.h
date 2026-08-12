//FpgaSimulator - программная модель загрузки пакетов и предпросмотра кадра ПЛИС

#pragma once

#include "fpga/packets/FpgaPacket.h"

#include <QByteArray>
#include <QImage>
#include <QMap>
#include <QStringList>

class FpgaSimulator
{
public:
    void loadBundle(const FpgaPacketBundle &bundle);
    QImage renderFrame() const;
    QStringList warnings() const { return m_warnings; }

private:
    FpgaPacketBundle m_bundle;
    QMap<int, QByteArray> m_parameters;
    QMap<int, QByteArray> m_staticMemory;
    QMap<int, QMap<int, QByteArray>> m_rotationMemory;
    QStringList m_warnings;

    void decodePackets();
};

