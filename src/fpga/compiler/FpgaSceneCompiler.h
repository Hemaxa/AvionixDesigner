// FpgaSceneCompiler - преобразование XML-модели и текста в аппаратную сцену.

#pragma once

#include "fpga/model/FpgaCompiledDocument.h"
#include "fpga/model/FpgaHardwareScene.h"

// FpgaSceneCompiler - преобразование XML-модели и текста в аппаратную сцену.
class FpgaSceneCompiler
{
public:
    // Раскрывает текст в SYMBOLs и назначает общие банки и индексы параметров.
    // При обновлении UART сохраняет загруженные шрифты и число позиций надписей.
    FpgaHardwareScene compile(const FpgaCompiledDocument &document, QString *errorMessage = nullptr,
                              const FpgaHardwareScene *loaded = nullptr) const;
};
