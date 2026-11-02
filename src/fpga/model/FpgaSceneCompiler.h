#pragma once

#include "fpga/model/FpgaCompiledDocument.h"
#include "fpga/model/FpgaHardwareScene.h"

class FpgaSceneCompiler
{
public:
    FpgaHardwareScene compile(const FpgaCompiledDocument &document, QString *errorMessage = nullptr) const;
};
