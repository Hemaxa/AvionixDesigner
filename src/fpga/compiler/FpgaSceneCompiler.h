// FpgaSceneCompiler - преобразование XML-модели и текста в аппаратную сцену.

#pragma once

#include "fpga/model/FpgaCompiledDocument.h"
#include "fpga/model/FpgaHardwareScene.h"

// FpgaSceneCompiler - преобразование XML-модели и текста в аппаратную сцену.
class FpgaSceneCompiler
{
public:
    // Раскрывает текст в staticgroup и назначает общие банки и индексы параметров.
    // Исходный документ не меняется; ошибка возвращает невалидную сцену без пакетов.
    FpgaHardwareScene compile(const FpgaCompiledDocument &document, QString *errorMessage = nullptr) const;
};
