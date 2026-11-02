#include "fpga/model/FpgaSceneCompiler.h"
#include "fpga/model/FpgaResourceLimits.h"

#include "BitParser.h"
#include "FpgaSchemaRegistry.h"
#include "text/TextLayout.h"

#include <QRect>

namespace {
constexpr int memoryCapacity = 65536; // Существующие адреса UART и STATICGROUP имеют 16 бит.
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

    QMap<int, int> fontMemoryIds;
    // Алфавит хранится целиком один раз, независимо от числа строк и повторений букв.
    for (auto it = document.fonts.cbegin(); it != document.fonts.cend(); ++it) {
        if (it.value().maskData.size() > memoryCapacity)
            return fail(QStringLiteral("Растр шрифта «%1», %2 px занимает %3 ячеек; 16-битная адресация допускает не более %4.")
                .arg(it.value().name).arg(it.value().size).arg(it.value().maskData.size()).arg(memoryCapacity));
        if (scene.memories.size() >= memoryBankCount)
            return fail(QStringLiteral("Сцена требует более 256 банков памяти; идентификатор банка имеет 8 бит."));
        fontMemoryIds.insert(it.key(), scene.memories.size());
        scene.memories.append({FpgaMemoryKind::StaticMask, it.value().maskData});
    }

    int nextParameter = 0;
    QMap<QString, int> croppedMasks;
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
                TextLayout layout;
                QString layoutError;
                if (!layoutText(characters.mid(field(hex, textSchema, "char_offset"), count), font, &layout, &layoutError))
                    return fail(layoutError);

                const QPoint origin(field(hex, textSchema, "x"), field(hex, textSchema, "y"));
                const int memId = fontMemoryIds.value(fontIndex);
                for (const auto &positioned : layout.glyphs) {
                    const auto &glyph = font.glyphs[positioned.character];
                    const qint64 left = qint64(origin.x()) + positioned.position.x();
                    const qint64 top = qint64(origin.y()) + positioned.position.y();
                    const qint64 right = qMin(qint64(scene.width), left + glyph.width);
                    const qint64 bottom = qMin(qint64(scene.height), top + glyph.height);
                    if (right <= left || bottom <= top)
                        continue;
                    if (left > 4095 || top > 4095 || right - left > 4095 || bottom - top > 4095)
                        return fail(QStringLiteral("Координаты или габариты символа U+%1 не помещаются в 12-битные поля STATICGROUP.")
                            .arg(QString::number(glyph.code, 16).toUpper()));
                    const QRect fullRect(int(left), int(top), glyph.width, glyph.height);
                    const QRect visible(int(left), int(top), int(right - left), int(bottom - top));

                    int address = glyph.offset;
                    if (visible != fullRect) {
                        // Нельзя просто уменьшить w: тогда меняется шаг строк растра в памяти.
                        const QRect crop(visible.topLeft() - fullRect.topLeft(), visible.size());
                        const QString key = QStringLiteral("%1/%2/%3/%4/%5/%6")
                            .arg(fontIndex).arg(glyph.code).arg(crop.x()).arg(crop.y()).arg(crop.width()).arg(crop.height());
                        if (crop.width() == glyph.width || crop.height() == 1) {
                            // Усечение по высоте и одна строка не меняют шаг чтения.
                            address += crop.y() * glyph.width + crop.x();
                        } else if (croppedMasks.contains(key)) {
                            address = croppedMasks.value(key);
                        } else {
                            auto &data = scene.memories[memId].data;
                            address = data.size();
                            if (qint64(address) + crop.width() * crop.height() > memoryCapacity)
                                return fail(QStringLiteral("Недостаточно адресуемой памяти для обрезанного символа U+%1 шрифта «%2».")
                                    .arg(QString::number(glyph.code, 16).toUpper(), font.name));
                            for (int y = 0; y < crop.height(); ++y)
                                data.append(font.maskData.mid(glyph.offset + (crop.y() + y) * glyph.width + crop.x(), crop.width()));
                            croppedMasks.insert(key, address);
                        }
                    }

                    if (nextParameter >= 65536)
                        return fail(QStringLiteral("Сцена требует более 65536 слов параметров; индекс параметра имеет 16 бит."));
                    FpgaHardwareObject object;
                    object.type = QStringLiteral("staticgroup");
                    object.memId = memId;
                    object.startParamIndex = nextParameter++;
                    object.iparams.append(staticInit(staticSchema, field(hex, textSchema, "enb") && glyph.enabled,
                        quint32(field(hex, textSchema, "color")), visible, address));
                    // Не объединяем буквы в одну маску и не меняем порядок слоёв.
                    // Назначение этих записей аппаратным модулям задаётся конфигурацией ПЛИС.
                    scene.objects.append(object);
                }
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
            if (source.type == "staticgroup" && source.iparams.size() > FpgaResourceLimits::staticParameters)
                return fail(QStringLiteral("Статическая группа содержит более 8 слов init. Разделите группу."));
            if (scene.memories.size() >= memoryBankCount)
                return fail(QStringLiteral("Сцена требует более 256 банков памяти; идентификатор банка имеет 8 бит."));
            if (source.type == "staticgroup" && source.data.size() > memoryCapacity)
                return fail(QStringLiteral("Статическая маска занимает более 65536 адресуемых ячеек."));
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
