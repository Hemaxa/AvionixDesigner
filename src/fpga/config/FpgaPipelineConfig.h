// FpgaPipelineConfig - структура аппаратного конвейера и экспорт tube_config_pkg.sv.

#pragma once

#include "fpga/model/FpgaHardwareScene.h"

// FpgaPipelineStage - тип, число 150-битных слов и RAMID одного этапа ПЛИС.
struct FpgaPipelineStage
{
    int type = 0;
    int parameterCount = 1;
    int ramId = 0;

    bool operator==(const FpgaPipelineStage &other) const {
        return type == other.type && parameterCount == other.parameterCount && ramId == other.ramId;
    }
};

// FpgaPipelineConfig - сохранённая схема этапов; значения параметров и маски сюда не входят.
struct FpgaPipelineConfig
{
    static constexpr int parameterArraySize = 200;
    QList<FpgaPipelineStage> stages;

    bool isValid() const;
    int parameterCount() const;
    bool operator==(const FpgaPipelineConfig &other) const { return stages == other.stages; }
    bool operator!=(const FpgaPipelineConfig &other) const { return !(*this == other); }
    static QString stageTypeName(int type);
    static FpgaPipelineConfig fromScene(const FpgaHardwareScene &scene, QString *error = nullptr);
    QByteArray systemVerilog() const;
    bool writeFile(const QString &path, QString *error = nullptr) const;
};
