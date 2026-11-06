// FpgaPipelineConfig - структура аппаратного конвейера и экспорт tube_config_pkg.sv.

#include "FpgaPipelineConfig.h"

#include <QSaveFile>

QString FpgaPipelineConfig::stageTypeName(int type)
{
    static const QStringList names = {"TYPE_RECTANGLE", "TYPE_ROTATIONOBJECT", "TYPE_STATICGROUP", "TYPE_RECTANGLEa",
        "TYPE_DASHLINE", "TYPE_AVIAHORIZONT", "TYPE_SPEEDSCALE", "TYPE_SYMBOLs"};
    return names.value(type);
}

int FpgaPipelineConfig::parameterCount() const
{
    int total = 0;
    for (const auto &stage : stages) {
        if (stage.parameterCount < 1 || stage.parameterCount > parameterArraySize || total > parameterArraySize - stage.parameterCount)
            return -1;
        total += stage.parameterCount;
    }
    return total;
}

bool FpgaPipelineConfig::isValid() const
{
    if (stages.isEmpty() || parameterCount() < 1) return false;
    for (const auto &stage : stages) {
        if (stageTypeName(stage.type).isEmpty() || stage.ramId < 0 || stage.ramId > 255) return false;
        const bool memory = stage.type == 1 || stage.type == 2 || stage.type == 7;
        if (!memory && stage.ramId != 0) return false;
        if (stage.type != 2 && stage.type != 7 && stage.parameterCount != 1) return false;
    }
    return true;
}

FpgaPipelineConfig FpgaPipelineConfig::fromScene(const FpgaHardwareScene &scene, QString *error)
{
    if (error) error->clear();
    auto fail = [error](const QString &message) {
        if (error) *error = message;
        return FpgaPipelineConfig{};
    };
    if (!scene.isValid()) return fail(QStringLiteral("Не задана аппаратная сцена для экспорта конфигурации."));
    static const QMap<QString, int> types = {{"rectangle", 0}, {"rotationobject", 1}, {"staticgroup", 2}, {"rectangle_a", 3},
        {"dashline", 4}, {"aviahorizont", 5}, {"RibonScale", 6}, {"symbols", 7}};
    FpgaPipelineConfig config;
    int nextParameter = 0;
    for (const auto &object : scene.objects) {
        // 3D-вид имеет отдельную команду UART и не входит в enum шаблона.
        if (object.type == "layer3d") continue;
        if (!types.contains(object.type)) return fail(QStringLiteral("Тип «%1» отсутствует в шаблоне конвейера ПЛИС.").arg(object.type));
        const int type = types.value(object.type);
        const int count = object.iparams.size();
        if (count < 1 || count > parameterArraySize - nextParameter)
            return fail(QStringLiteral("Конвейер превышает PARAM_ARRAY_SIZE = %1 или содержит этап без параметров.").arg(parameterArraySize));
        if (object.startParamIndex != nextParameter)
            return fail(QStringLiteral("Индексы параметров сцены не соответствуют последовательному распределению конвейера."));
        const bool memory = type == 1 || type == 2 || type == 7;
        if (memory && (object.memId < 0 || object.memId >= scene.memories.size() || object.memId > 255))
            return fail(QStringLiteral("У растрового этапа отсутствует допустимый RAMID."));
        if (memory && scene.memories[object.memId].kind != (type == 1 ? FpgaMemoryKind::RotationTiles : FpgaMemoryKind::StaticMask))
            return fail(QStringLiteral("Тип памяти не соответствует аппаратному этапу."));
        if (!memory && object.memId != -1) return fail(QStringLiteral("Этап без растровой памяти ссылается на банк."));
        const int ramId = memory ? object.memId : 0;
        // STATICGROUP/SYMBOLs получают вектор init. Остальные RTL-модули
        // принимают одно слово: состояния разворачиваются в последовательные этапы.
        if (type == 2 || type == 7) config.stages.append({type, count, ramId});
        else for (int i = 0; i < count; ++i) config.stages.append({type, 1, ramId});
        nextParameter += count;
    }
    if (!config.isValid()) return fail(QStringLiteral("В сцене нет экспортируемых этапов конвейера ПЛИС."));
    return config;
}

QByteArray FpgaPipelineConfig::systemVerilog() const
{
    if (!isValid()) return {};
    QString text = QStringLiteral("package tube_config_pkg;\n\n    typedef enum int {\n");
    for (int i = 0; i < 8; ++i)
        text += QStringLiteral("        %1 = %2%3\n").arg(stageTypeName(i)).arg(i).arg(i == 7 ? "" : ",");
    text += QStringLiteral("    } stage_type_t;\n\n    localparam int N_STAGES = %1;\n    localparam int PARAM_ARRAY_SIZE = %2;\n\n")
        .arg(stages.size()).arg(parameterArraySize);
    auto array = [&](const QString &type, const QString &name, int field) {
        text += QStringLiteral("    localparam %1 %2 [0:N_STAGES-1] = '{\n").arg(type, name);
        for (int i = 0; i < stages.size(); ++i) {
            const auto &stage = stages[i];
            const QString value = field == 0 ? stageTypeName(stage.type) : QString::number(field == 1 ? stage.parameterCount : stage.ramId);
            text += QStringLiteral("        %1%2 // sec%3\n").arg(value, i + 1 == stages.size() ? "" : ",")
                .arg(i, 4, 10, QLatin1Char('0'));
        }
        text += QStringLiteral("    };\n\n");
    };
    array("stage_type_t", "STAGE_TYPE", 0);
    array("int", "STAGE_PARAM_COUNT", 1);
    array("int", "STAGE_RAMID", 2);
    text += QStringLiteral("endpackage\n");
    return text.toUtf8();
}

bool FpgaPipelineConfig::writeFile(const QString &path, QString *error) const
{
    if (error) error->clear();
    auto fail = [error](const QString &message) { if (error) *error = message; return false; };
    const auto bytes = systemVerilog();
    if (bytes.isEmpty()) return fail(QStringLiteral("Нельзя сохранить пустую или неверную конфигурацию ПЛИС."));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(QStringLiteral("Не удалось открыть конфигурацию: %1").arg(file.errorString()));
    if (file.write(bytes) != bytes.size()) return fail(QStringLiteral("Не удалось записать конфигурацию: %1").arg(file.errorString()));
    if (!file.commit()) return fail(QStringLiteral("Не удалось сохранить конфигурацию: %1").arg(file.errorString()));
    return true;
}
