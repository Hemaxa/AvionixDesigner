// FpgaSceneCompiler - преобразование XML-модели и текста в аппаратную сцену.

#include "fpga/compiler/FpgaSceneCompiler.h"
#include "fpga/compiler/FpgaResourceLimits.h"

#include "BitParser.h"
#include "FpgaSchemaRegistry.h"
#include "text/TextLayout.h"

#include <QRect>

namespace {
constexpr int memoryBankCount = 256;

int field(const QString &hex, const ParamSchema &schema, const QString &name)
{
    const auto info = schema.value(name);
    return int(BitParser::extract(hex, info.offset, info.size));
}

QString staticInit(const ParamSchema &schema, bool enabled, quint32 color, const QRect &rect, int address)
{
    QString hex(38, '0');
    const QMap<QString, quint32> values = {
        {"enb", enabled ? 1u : 0u}, {"color", color},
        {"x", quint32(rect.x())}, {"y", quint32(rect.y())},
        {"w", quint32(rect.width())}, {"h", quint32(rect.height())}, {"addr", quint32(address)}
    };
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const auto info = schema.value(it.key());
        BitParser::inject(hex, info.offset, info.size, it.value());
    }
    return hex;
}
}

FpgaHardwareScene FpgaSceneCompiler::compile(const FpgaCompiledDocument &document, QString *errorMessage) const
{
    if (errorMessage)
        errorMessage->clear();
    auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return FpgaHardwareScene{};
    };
    if (!document.isValid())
        return fail(QStringLiteral("Нельзя подготовить аппаратную сцену: неверные размеры проекта."));

    FpgaHardwareScene scene;
    scene.projectName = document.projectName;
    scene.width = document.width;
    scene.height = document.height;
    scene.backgroundColor = document.backgroundColor;
    scene.schemas = document.schemas;
    scene.schemas.remove("font");
    scene.schemas.remove("text");
    const auto staticSchema = FpgaSchemaRegistry::instance()->buildSchema("staticgroup");
    if (!document.fonts.isEmpty())
        scene.schemas.insert("staticgroup", staticSchema);
    scene.warnings = document.warnings;

    const int memoryCapacity = FpgaResourceLimits::staticPixels();
    int nextParameter = 0;
    const auto textSchema = document.schemas.value("text");
    for (const auto &source : document.objects) {
        if (source.type == QStringLiteral("text")) {
            QString characters;
            for (const auto &code : source.data.split(',', Qt::SkipEmptyParts))
                characters.append(QChar(static_cast<ushort>(code.toUInt())));

            for (const auto &hex : source.iparams) {
                const int count = field(hex, textSchema, "char_count");
                if (count == 0)
                    continue;
                const int fontIndex = field(hex, textSchema, "font_index");
                const auto fontIt = document.fonts.constFind(fontIndex);
                if (fontIt == document.fonts.cend())
                    return fail(QStringLiteral("В аппаратной сцене отсутствует шрифт %1.").arg(fontIndex));
                const auto &font = fontIt.value();
                TextMask mask;
                QString layoutError;
                if (!composeTextMask(characters.mid(field(hex, textSchema, "char_offset"), count), font, &mask, &layoutError, memoryCapacity))
                    return fail(layoutError);
                if (mask.data.isEmpty()) continue;
                const QPoint origin(field(hex, textSchema, "x"), field(hex, textSchema, "y"));
                const QRect fullRect(origin + mask.offset, mask.size);
                const QRect visible = fullRect.intersected(QRect(0, 0, scene.width, scene.height));
                if (visible.isEmpty()) continue;
                if (visible.x() > 4095 || visible.y() > 4095 || visible.width() > 4095 || visible.height() > 4095)
                    return fail(QStringLiteral("Координаты или габариты текста не помещаются в 12-битные поля STATICGROUP."));
                if (scene.memories.size() >= memoryBankCount)
                    return fail(QStringLiteral("Сцена требует более 256 банков памяти; идентификатор банка имеет 8 бит."));
                if (nextParameter >= 65536)
                    return fail(QStringLiteral("Сцена требует более 65536 слов параметров; индекс параметра имеет 16 бит."));
                QString data;
                const QPoint crop = visible.topLeft() - fullRect.topLeft();
                for (int y = 0; y < visible.height(); ++y)
                    data.append(mask.data.mid((crop.y() + y) * mask.size.width() + crop.x(), visible.width()));
                FpgaHardwareObject object;
                object.type = QStringLiteral("staticgroup");
                object.memId = scene.memories.size();
                object.startParamIndex = nextParameter++;
                object.iparams.append(staticInit(staticSchema, field(hex, textSchema, "enb"),
                    quint32(field(hex, textSchema, "color")), visible, 0));
                scene.memories.append({FpgaMemoryKind::StaticMask, data});
                scene.objects.append(object);
            }
            continue;
        }

        FpgaHardwareObject object;
        object.type = source.type;
        object.leftAnchored = source.leftAnchored;
        object.iparams = source.iparams;
        object.startParamIndex = nextParameter;
        if (source.type != QStringLiteral("layer3d")) {
            if (nextParameter + object.iparams.size() > 65536)
                return fail(QStringLiteral("Сцена требует более 65536 слов параметров; индекс параметра имеет 16 бит."));
            nextParameter += object.iparams.size();
        }
        if (source.type == "staticgroup" || source.type == "rotationobject") {
            if (source.type == "staticgroup" && source.iparams.size() > FpgaResourceLimits::staticParameters())
                return fail(QStringLiteral("Статическая группа содержит более %1 слов init. Разделите группу.").arg(FpgaResourceLimits::staticParameters()));
            if (scene.memories.size() >= memoryBankCount)
                return fail(QStringLiteral("Сцена требует более 256 банков памяти; идентификатор банка имеет 8 бит."));
            if (source.type == "staticgroup" && source.data.size() > memoryCapacity)
                return fail(QStringLiteral("Статическая маска занимает более %1 адресуемых ячеек.").arg(memoryCapacity));
            if (source.type == "staticgroup") {
                for (const auto &hex : source.iparams) {
                    if (!field(hex, staticSchema, "enb"))
                        continue;
                    const qint64 end = field(hex, staticSchema, "addr")
                        + qint64(field(hex, staticSchema, "w")) * field(hex, staticSchema, "h");
                    if (end > source.data.size() || end > memoryCapacity)
                        return fail(QStringLiteral("Растр статического объекта выходит за границы адресуемой памяти или <data>."));
                }
            }
            object.memId = scene.memories.size();
            scene.memories.append({source.type == "staticgroup" ? FpgaMemoryKind::StaticMask : FpgaMemoryKind::RotationTiles, source.data});
        }
        scene.objects.append(object);
    }
    return scene;
}
