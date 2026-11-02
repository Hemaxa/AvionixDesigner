//FpgaPacketCompiler - упаковывает готовую аппаратную сцену в UART/BIN пакеты.

#include "fpga/packets/FpgaPacketCompiler.h"

#include "BitParser.h"
#include "fpga/packets/FpgaPacketFramer.h"
#include "fpga/model/FpgaResourceLimits.h"

#include <QtGlobal>
#include <QSet>
#include <algorithm>

namespace {
void appendUint16(QByteArray *data, quint16 value)
{
    data->append(static_cast<char>((value >> 8) & 0xFF));
    data->append(static_cast<char>(value & 0xFF));
}

QByteArray hexToBytes(QString hex)
{
    hex.remove(QLatin1Char(' '));
    hex.remove(QLatin1Char('\t'));
    hex.remove(QLatin1Char('\n'));
    hex.remove(QLatin1Char('\r'));
    if (hex.size() % 2 != 0)
        hex.prepend(QLatin1Char('0'));
    return QByteArray::fromHex(hex.toLatin1());
}

QVector<int> digitsToMaskValues(const QString &data, QStringList *warnings)
{
    QVector<int> values;
    values.reserve(data.size());
    for (const QChar ch : data) {
        if (!ch.isDigit())
            continue;
        const int value = ch.digitValue();
        if (value < 0 || value > 7) {
            if (warnings)
                warnings->append(QStringLiteral("Mask value '%1' is outside 0..7 and was clamped.").arg(ch));
            values.append(qBound(0, value, 7));
            continue;
        }
        values.append(value);
    }
    return values;
}

int extractSignedField(const QString &hex, const ParamSchema &schema, const QString &fieldName)
{
    if (!schema.contains(fieldName))
        return 0;
    const ParamInfo info = schema.value(fieldName);
    return BitParser::extractSigned(hex, info.offset, info.size);
}

QByteArray packRotationTile(const QVector<int> &tileValues)
{
    QByteArray bits;
    bits.reserve(248);
    for (int value : tileValues) {
        value = qBound(0, value, 7);
        bits.append((value & 0x01) ? '1' : '0');
        bits.append((value & 0x02) ? '1' : '0');
        bits.append((value & 0x04) ? '1' : '0');
    }
    bits.append("00000", 5);

    QByteArray out(31, char(0));
    const int bitCount = qMin(bits.size(), 248);
    for (int i = 0; i < bitCount; ++i) {
        if (bits.at(bitCount - 1 - i) != '1')
            continue;
        const int byteIndex = i / 8;
        const int bitIndex = 7 - (i % 8);
        out[byteIndex] = static_cast<char>(static_cast<quint8>(out[byteIndex]) | (1u << bitIndex));
    }
    return out;
}

QByteArray framePacket(FpgaPacketType type, const QByteArray &payload, quint32 *fcsOut)
{
    return FpgaPacketFramer::frame(static_cast<quint8>(type), payload, fcsOut);
}
}

FpgaPacketBundle FpgaPacketCompiler::compile(const FpgaHardwareScene &document, QString *errorMessage) const
{
    if (errorMessage)
        errorMessage->clear();
    auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return FpgaPacketBundle{};
    };
    if (!document.isValid())
        return fail(QStringLiteral("Некорректная аппаратная сцена."));
    if (document.memories.size() > 256)
        return fail(QStringLiteral("Идентификатор банка памяти не помещается в 8 бит."));
    for (const auto &memory : document.memories) {
        if (memory.kind == FpgaMemoryKind::StaticMask && memory.data.size() > 65536)
            return fail(QStringLiteral("Статическая память превышает 65536 адресуемых ячеек."));
        for (const auto level : memory.data)
            if (level < QLatin1Char('0') || level > QLatin1Char('7'))
                return fail(QStringLiteral("Растр должен содержать только уровни маски 0…7."));
    }

    FpgaPacketBundle bundle;
    bundle.document = document;
    bundle.warnings = document.warnings;
    QSet<int> uploadedMemories;
    auto appendMemory = [&](int memId, int objectIndex, const QString &type, const QList<QByteArray> &blocks) {
        for (int address = 0; address < blocks.size(); ++address) {
            FpgaPacket packet;
            packet.type = FpgaPacketType::RotationMemory;
            packet.objectType = type;
            packet.objectIndex = objectIndex;
            packet.memId = memId;
            packet.address = address;
            packet.payloadSize = blocks[address].size();
            packet.bytes = framePacket(packet.type, memoryPayload(memId, address, blocks[address]), &packet.fcs);
            bundle.packets.append(packet);
        }
    };
    auto appendStaticMemory = [&](int memId, int objectIndex, const QString &type) {
        const QByteArray data = encodeStaticMaskData(document.memories[memId].data, &bundle.warnings);
        for (int address = 0; address < data.size(); address += 1024) {
            const QByteArray chunk = data.mid(address, 1024);
            FpgaPacket packet;
            packet.type = FpgaPacketType::StaticMemory;
            packet.objectType = type;
            packet.objectIndex = objectIndex;
            packet.memId = memId;
            packet.address = address;
            packet.payloadSize = chunk.size();
            packet.bytes = framePacket(packet.type, memoryPayload(memId, address, chunk), &packet.fcs);
            bundle.packets.append(packet);
        }
    };

    for (int objectIndex = 0; objectIndex < document.objects.size(); ++objectIndex) {
        const auto &object = document.objects[objectIndex];
        if (object.type == QStringLiteral("font") || object.type == QStringLiteral("text"))
            return fail(QStringLiteral("Перед упаковкой текст должен быть преобразован в статические объекты."));
        if (object.type == QStringLiteral("layer3d"))
            continue;
        if (object.startParamIndex < 0 || object.startParamIndex + object.iparams.size() > 65536)
            return fail(QStringLiteral("Индекс параметра не помещается в 16 бит."));

        for (int paramIndex = 0; paramIndex < object.iparams.size(); ++paramIndex) {
            const int objId = object.startParamIndex + paramIndex;
            const QByteArray paramBytes = hexToBytes(object.iparams[paramIndex]);
            if (paramBytes.size() != 19)
                return fail(QStringLiteral("Слово параметров должно занимать 19 байт (150 бит)."));
            FpgaPacket packet;
            packet.type = FpgaPacketType::Parameter;
            packet.objectType = object.type;
            packet.objectIndex = objectIndex;
            packet.parameterIndex = paramIndex;
            packet.objId = objId;
            packet.payloadSize = paramBytes.size();
            packet.bytes = framePacket(packet.type, parameterPayload(objId, paramBytes), &packet.fcs);
            bundle.packets.append(packet);
        }

        if (object.memId == -1)
            continue;
        if (object.memId < 0 || object.memId >= document.memories.size())
            return fail(QStringLiteral("У объекта некорректный идентификатор банка памяти."));
        if (uploadedMemories.contains(object.memId))
            continue;
        uploadedMemories.insert(object.memId);
        if (document.memories[object.memId].kind == FpgaMemoryKind::RotationTiles) {
            const auto blocks = encodeRotationTiles(document, object, &bundle.warnings);
            if (blocks.size() > FpgaResourceLimits::rotationTiles)
                return fail(QStringLiteral("Число тайлов вращаемой маски превышает ёмкость памяти: 2048 тайлов."));
            appendMemory(object.memId, objectIndex, object.type, blocks);
        } else {
            appendStaticMemory(object.memId, objectIndex, object.type);
        }
    }
    // Алфавит нужен и тогда, когда строка пока пустая или содержит только пробелы.
    for (int memId = 0; memId < document.memories.size(); ++memId)
        if (!uploadedMemories.contains(memId) && document.memories[memId].kind == FpgaMemoryKind::StaticMask)
            appendStaticMemory(memId, -1, QStringLiteral("staticgroup"));

    // После загрузки сцены идут команда стороны 3D-вида и первое положение самолета.
    for (int i = 0; i < document.objects.size(); ++i) {
        const auto &object = document.objects[i];
        if (object.type != QStringLiteral("layer3d") || object.iparams.isEmpty()) continue;
        auto side = layer3dSidePacket(object.leftAnchored);
        side.objectIndex = i;
        bundle.packets.append(side);
        auto position = layer3dPositionPacket(object.iparams.first());
        position.objectIndex = i;
        bundle.packets.append(position);
        break;
    }
    return bundle;
}

QByteArray FpgaPacketCompiler::parameterPayload(int objId, const QByteArray &data)
{
    QByteArray payload;
    payload.reserve(4 + data.size());
    appendUint16(&payload, static_cast<quint16>(objId));
    appendUint16(&payload, static_cast<quint16>(data.size()));
    payload.append(data);
    return payload;
}

QByteArray FpgaPacketCompiler::memoryPayload(int memId, int address, const QByteArray &data)
{
    QByteArray payload;
    payload.reserve(5 + data.size());
    payload.append(static_cast<char>(memId & 0xFF));
    appendUint16(&payload, static_cast<quint16>(address));
    appendUint16(&payload, static_cast<quint16>(data.size()));
    payload.append(data);
    return payload;
}

QByteArray FpgaPacketCompiler::layer3dPayload(const QByteArray &data)
{
    // XML хранит число в обычном порядке HEX; эталон pack_150bit передает
    // 19 байт от младшего к старшему. Порядок байтов остальных объектов не меняется.
    QByteArray payload = data;
    std::reverse(payload.begin(), payload.end());
    return payload;
}

FpgaPacket FpgaPacketCompiler::layer3dSidePacket(bool left)
{
    FpgaPacket packet;
    packet.type = FpgaPacketType::Layer3d;
    packet.objectType = QStringLiteral("layer3d");
    packet.payloadSize = 1;
    packet.bytes = framePacket(packet.type, QByteArray(1, left ? char(1) : char(0)), &packet.fcs);
    return packet;
}

FpgaPacket FpgaPacketCompiler::layer3dPositionPacket(const QString &initHex)
{
    FpgaPacket packet;
    packet.type = FpgaPacketType::Layer3d;
    packet.objectType = QStringLiteral("layer3d");
    packet.payloadSize = 19;
    packet.parameterIndex = 0;
    const QByteArray data = hexToBytes(initHex);
    if (data.size() != 19) return packet;
    packet.bytes = framePacket(packet.type, layer3dPayload(data), &packet.fcs);
    return packet;
}

QList<QByteArray> FpgaPacketCompiler::encodeRotationTiles(const FpgaHardwareScene &document,
                                                          const FpgaHardwareObject &object,
                                                          QStringList *warnings) const
{
    QList<QByteArray> blocks;
    if (object.iparams.isEmpty())
        return blocks;

    const ParamSchema schema = document.schemas.value(QStringLiteral("rotationobject"));
    if (schema.isEmpty()) {
        if (warnings)
            warnings->append(QStringLiteral("rotationobject schema is missing; rotation memory was not encoded."));
        return blocks;
    }

    const QString iparam = object.iparams.first();
    const int left = extractSignedField(iparam, schema, QStringLiteral("left"));
    const int right = extractSignedField(iparam, schema, QStringLiteral("right"));
    const int top = extractSignedField(iparam, schema, QStringLiteral("top"));
    const int bottom = extractSignedField(iparam, schema, QStringLiteral("bottom"));
    const int width = right - left;
    const int height = bottom - top;
    if (width <= 0 || height <= 0) {
        if (warnings)
            warnings->append(QStringLiteral("rotationobject has invalid bounds; rotation memory was not encoded."));
        return blocks;
    }

    QVector<int> source = digitsToMaskValues(document.memories[object.memId].data, warnings);
    const int expectedSize = width * height;
    if (source.size() < expectedSize) {
        if (warnings)
            warnings->append(QStringLiteral("rotationobject data is shorter than bounds; missing mask values were filled with zero."));
        source.resize(expectedSize);
    }

    const int tilesX = (width + 7) / 8;
    const int tilesY = (height + 7) / 8;
    blocks.reserve(tilesX * tilesY);

    for (int tileY = 0; tileY < tilesY; ++tileY) {
        for (int tileX = 0; tileX < tilesX; ++tileX) {
            QVector<int> tile;
            tile.reserve(81);
            for (int row = 0; row < 9; ++row) {
                const int srcY = tileY * 8 + (row == 8 ? 7 : row);
                for (int col = 0; col < 9; ++col) {
                    const int srcX = tileX * 8 + (col == 8 ? 7 : col);
                    if (srcX >= width || srcY >= height) {
                        tile.append(0);
                    } else {
                        tile.append(source[srcY * width + srcX]);
                    }
                }
            }
            blocks.append(packRotationTile(tile));
        }
    }

    return blocks;
}

QByteArray FpgaPacketCompiler::encodeStaticMaskData(const QString &data, QStringList *warnings) const
{
    const QVector<int> values = digitsToMaskValues(data, warnings);
    QByteArray bytes;
    bytes.reserve(values.size());
    for (int value : values)
        bytes.append(static_cast<char>(qBound(0, value, 7)));
    return bytes;
}
