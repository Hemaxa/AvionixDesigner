//FpgaPacketCompiler - компилирует строгий FPGA-документ в UART/BIN пакеты

#include "fpga/packets/FpgaPacketCompiler.h"

#include "BitParser.h"
#include "fpga/packets/FpgaPacketFramer.h"

#include <QtGlobal>

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

FpgaPacketBundle FpgaPacketCompiler::compile(const FpgaCompiledDocument &document) const
{
    FpgaPacketBundle bundle;
    bundle.document = document;
    bundle.warnings = document.warnings;

    for (int objectIndex = 0; objectIndex < document.objects.size(); ++objectIndex) {
        const FpgaCompiledObject &object = document.objects[objectIndex];

        for (int paramIndex = 0; paramIndex < object.iparams.size(); ++paramIndex) {
            const int objId = object.startParamIndex + paramIndex;
            const QByteArray paramBytes = hexToBytes(object.iparams[paramIndex]);
            quint32 fcs = 0;
            FpgaPacket packet;
            packet.type = FpgaPacketType::Parameter;
            packet.objectType = object.type;
            packet.objectIndex = objectIndex;
            packet.parameterIndex = paramIndex;
            packet.objId = objId;
            packet.payloadSize = paramBytes.size();
            packet.bytes = framePacket(packet.type, parameterPayload(objId, paramBytes), &fcs);
            packet.fcs = fcs;
            bundle.packets.append(packet);
        }

        if (object.memoryKind == FpgaMemoryKind::RotationTiles) {
            const QList<QByteArray> blocks = encodeRotationTiles(document, object, &bundle.warnings);
            for (int address = 0; address < blocks.size(); ++address) {
                quint32 fcs = 0;
                FpgaPacket packet;
                packet.type = FpgaPacketType::RotationMemory;
                packet.objectType = object.type;
                packet.objectIndex = objectIndex;
                packet.memId = object.memId;
                packet.address = address;
                packet.payloadSize = blocks[address].size();
                packet.bytes = framePacket(packet.type, memoryPayload(object.memId, address, blocks[address]), &fcs);
                packet.fcs = fcs;
                bundle.packets.append(packet);
            }
        } else if (object.memoryKind == FpgaMemoryKind::StaticMask) {
            const QByteArray data = encodeStaticMaskData(object.data, &bundle.warnings);
            for (int address = 0; address < data.size(); address += 1024) {
                const QByteArray chunk = data.mid(address, 1024);
                quint32 fcs = 0;
                FpgaPacket packet;
                packet.type = FpgaPacketType::StaticMemory;
                packet.objectType = object.type;
                packet.objectIndex = objectIndex;
                packet.memId = object.memId;
                packet.address = address;
                packet.payloadSize = chunk.size();
                packet.bytes = framePacket(packet.type, memoryPayload(object.memId, address, chunk), &fcs);
                packet.fcs = fcs;
                bundle.packets.append(packet);
            }
        }
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
    return data;
}

QList<QByteArray> FpgaPacketCompiler::encodeRotationTiles(const FpgaCompiledDocument &document,
                                                          const FpgaCompiledObject &object,
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

    QVector<int> source = digitsToMaskValues(object.data, warnings);
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

