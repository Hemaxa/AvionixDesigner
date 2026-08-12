//FpgaProjectPacketBuilder - единая точка сборки текущего проекта в пакетный bundle

#pragma once

#include "fpga/packets/FpgaPacket.h"

class QString;

class FpgaProjectPacketBuilder
{
public:
    static FpgaPacketBundle buildCurrentProject(QString *errorMessage = nullptr);
};

