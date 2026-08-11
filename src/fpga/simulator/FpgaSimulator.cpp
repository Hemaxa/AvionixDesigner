//FpgaSimulator - программная модель загрузки пакетов и предпросмотра кадра ПЛИС

#include "fpga/simulator/FpgaSimulator.h"

#include "BitParser.h"

#include <QPainter>
#include <QRegularExpression>
#include <QtMath>

#include <algorithm>

namespace {
struct SimGlyph
{
    uint code = 0;
    int width = 0;
    int height = 0;
    int advance = 0;
    int bearingX = 0;
    int bearingY = 0;
    int ascent = 0;
    int descent = 0;
    int offset = 0;
    int maskSize = 0;
};

struct SimFont
{
    int index = 0;
    int spaceAdvance = 4;
    QByteArray memory;
    QMap<uint, SimGlyph> glyphs;
    QMap<QString, int> kerningPairs;
};

quint8 byteAt(const QByteArray &bytes, int index)
{
    return static_cast<quint8>(bytes.at(index));
}

quint16 readUint16(const QByteArray &bytes, int index)
{
    return static_cast<quint16>((byteAt(bytes, index) << 8) | byteAt(bytes, index + 1));
}

QString bytesToHex(const QByteArray &bytes)
{
    return QString::fromLatin1(bytes.toHex().toUpper());
}

quint32 extractField(const QString &hex, const ParamSchema &schema, const QString &fieldName)
{
    if (!schema.contains(fieldName))
        return 0;
    const ParamInfo info = schema.value(fieldName);
    return BitParser::extract(hex, info.offset, info.size);
}

qint32 extractSignedField(const QString &hex, const ParamSchema &schema, const QString &fieldName)
{
    if (!schema.contains(fieldName))
        return 0;
    const ParamInfo info = schema.value(fieldName);
    return BitParser::extractSigned(hex, info.offset, info.size);
}

QColor colorFromBgr(quint32 value)
{
    return QColor(
        static_cast<int>(value & 0xFF),
        static_cast<int>((value >> 8) & 0xFF),
        static_cast<int>((value >> 16) & 0xFF)
    );
}

bool isSpaceCode(uint code)
{
    return code == QLatin1Char(' ').unicode()
        || code == QLatin1Char('\t').unicode()
        || code == QLatin1Char('\n').unicode()
        || code == QLatin1Char('\r').unicode();
}

QString kerningKey(uint left, uint right)
{
    if (left > 0xFFFF || right > 0xFFFF)
        return {};
    return QString(QChar(static_cast<ushort>(left))) + QString(QChar(static_cast<ushort>(right)));
}

QList<uint> parseCharacterCodes(const QString &data)
{
    QList<uint> codes;
    const QStringList parts = data.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
    codes.reserve(parts.size());
    for (const QString &part : parts) {
        bool ok = false;
        const uint code = part.trimmed().toUInt(&ok);
        if (ok)
            codes.append(code);
    }
    return codes;
}

void blendPixel(QImage *image, int x, int y, const QColor &color, int mask)
{
    if (!image || mask <= 0 || x < 0 || y < 0 || x >= image->width() || y >= image->height())
        return;

    mask = qBound(0, mask, 7);
    const int weight = mask + 1;
    const QColor dst = image->pixelColor(x, y);
    const int inv = 8 - weight;
    image->setPixelColor(x, y, QColor(
        (color.red() * weight + dst.red() * inv) / 8,
        (color.green() * weight + dst.green() * inv) / 8,
        (color.blue() * weight + dst.blue() * inv) / 8
    ));
}

void blendPixel256(QImage *image, int x, int y, const QColor &color, int mask)
{
    if (!image || x < 0 || y < 0 || x >= image->width() || y >= image->height())
        return;

    mask = qBound(0, mask, 256);
    const QColor dst = image->pixelColor(x, y);
    const int inv = 256 - mask;
    image->setPixelColor(x, y, QColor(
        (color.red() * mask + dst.red() * inv) >> 8,
        (color.green() * mask + dst.green() * inv) >> 8,
        (color.blue() * mask + dst.blue() * inv) >> 8
    ));
}

QMap<int, SimFont> buildFonts(const FpgaCompiledDocument &document,
                              const QMap<int, QByteArray> &parameters,
                              const QMap<int, QByteArray> &staticMemory)
{
    QMap<int, SimFont> fonts;
    const ParamSchema schema = document.schemas.value(QStringLiteral("font"));
    if (schema.isEmpty())
        return fonts;

    for (const FpgaCompiledObject &object : document.objects) {
        if (object.type != QStringLiteral("font"))
            continue;

        for (int i = 0; i < object.paramCount; ++i) {
            const QString paramHex = bytesToHex(parameters.value(object.startParamIndex + i));
            if (paramHex.isEmpty() || extractField(paramHex, schema, QStringLiteral("enb")) == 0)
                continue;

            SimGlyph glyph;
            glyph.code = extractField(paramHex, schema, QStringLiteral("code"));
            glyph.width = static_cast<int>(extractField(paramHex, schema, QStringLiteral("w")));
            glyph.height = static_cast<int>(extractField(paramHex, schema, QStringLiteral("h")));
            glyph.advance = static_cast<int>(extractField(paramHex, schema, QStringLiteral("advance")));
            glyph.bearingX = extractSignedField(paramHex, schema, QStringLiteral("bearing_x"));
            glyph.bearingY = extractSignedField(paramHex, schema, QStringLiteral("bearing_y"));
            glyph.ascent = static_cast<int>(extractField(paramHex, schema, QStringLiteral("ascent")));
            glyph.descent = static_cast<int>(extractField(paramHex, schema, QStringLiteral("descent")));
            glyph.offset = static_cast<int>(extractField(paramHex, schema, QStringLiteral("offset")));
            glyph.maskSize = static_cast<int>(extractField(paramHex, schema, QStringLiteral("mask_size")));
            if (glyph.maskSize <= 0)
                glyph.maskSize = qMax(0, glyph.width * glyph.height);
            if (glyph.advance <= 0)
                glyph.advance = qMax(1, glyph.width);

            const int fontIndex = static_cast<int>(extractField(paramHex, schema, QStringLiteral("font_index")));
            SimFont &font = fonts[fontIndex];
            font.index = fontIndex;
            font.memory = staticMemory.value(object.memId);
            font.kerningPairs = object.kerningPairs;
            if (isSpaceCode(glyph.code))
                font.spaceAdvance = qMax(1, glyph.advance);
            if (glyph.code > 0 && glyph.width > 0 && glyph.height > 0)
                font.glyphs.insert(glyph.code, glyph);
        }
    }

    return fonts;
}

QVector<int> unpackRotationWord(const QByteArray &word)
{
    QVector<int> values;
    values.reserve(81);
    if (word.size() < 31)
        return values;

    auto encodedBit = [&word](int bitIndex) {
        const int byteIndex = bitIndex / 8;
        const int bitInByte = 7 - (bitIndex % 8);
        return (byteAt(word, byteIndex) >> bitInByte) & 0x01;
    };

    QVector<int> bits;
    bits.reserve(248);
    for (int i = 0; i < 248; ++i)
        bits.append(encodedBit(247 - i));

    for (int i = 0; i < 81; ++i) {
        const int base = i * 3;
        values.append(bits[base] | (bits[base + 1] << 1) | (bits[base + 2] << 2));
    }
    return values;
}

QImage rotationMaskFromMemory(const QString &paramHex,
                              const ParamSchema &schema,
                              const QMap<int, QByteArray> &memory)
{
    const int left = extractSignedField(paramHex, schema, QStringLiteral("left"));
    const int right = extractSignedField(paramHex, schema, QStringLiteral("right"));
    const int top = extractSignedField(paramHex, schema, QStringLiteral("top"));
    const int bottom = extractSignedField(paramHex, schema, QStringLiteral("bottom"));
    const int width = right - left;
    const int height = bottom - top;
    if (width <= 0 || height <= 0)
        return {};

    QVector<int> mask(width * height, 0);
    const int tilesX = (width + 7) / 8;
    const int tilesY = (height + 7) / 8;
    for (int tileY = 0; tileY < tilesY; ++tileY) {
        for (int tileX = 0; tileX < tilesX; ++tileX) {
            const int address = tileY * tilesX + tileX;
            const QVector<int> tile = unpackRotationWord(memory.value(address));
            if (tile.size() < 81)
                continue;

            for (int y = 0; y < 8; ++y) {
                const int dstY = tileY * 8 + y;
                if (dstY >= height)
                    continue;
                for (int x = 0; x < 8; ++x) {
                    const int dstX = tileX * 8 + x;
                    if (dstX >= width)
                        continue;
                    mask[dstY * width + dstX] = tile[y * 9 + x];
                }
            }
        }
    }

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    const QColor color = colorFromBgr(extractField(paramHex, schema, QStringLiteral("color")));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int value = qBound(0, mask[y * width + x], 7);
            QColor pixel = color;
            pixel.setAlpha(qRound(value * 255.0 / 7.0));
            image.setPixelColor(x, y, pixel);
        }
    }
    return image;
}

void renderRectangle(QImage *frame, const QString &paramHex, const ParamSchema &schema, bool alphaMode)
{
    if (!frame || extractField(paramHex, schema, QStringLiteral("enb")) == 0)
        return;

    QColor color = colorFromBgr(extractField(paramHex, schema, QStringLiteral("color")));
    if (alphaMode) {
        const int alph = extractField(paramHex, schema, QStringLiteral("alph"));
        color.setAlpha(qRound(qBound(1, alph + 1, 8) * 255.0 / 8.0));
    }

    QPainter painter(frame);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRect(QRect(
        static_cast<int>(extractField(paramHex, schema, QStringLiteral("x0"))),
        static_cast<int>(extractField(paramHex, schema, QStringLiteral("y0"))),
        qMax(1, static_cast<int>(extractField(paramHex, schema, QStringLiteral("w")))),
        qMax(1, static_cast<int>(extractField(paramHex, schema, QStringLiteral("h"))))
    ));
}

void renderStaticGroup(QImage *frame,
                       const FpgaCompiledObject &object,
                       const QMap<int, QByteArray> &parameters,
                       const QByteArray &memory,
                       const ParamSchema &schema)
{
    if (!frame || schema.isEmpty())
        return;

    for (int i = 0; i < object.paramCount; ++i) {
        const QString paramHex = bytesToHex(parameters.value(object.startParamIndex + i));
        if (paramHex.isEmpty() || extractField(paramHex, schema, QStringLiteral("enb")) == 0)
            continue;

        const QColor color = colorFromBgr(extractField(paramHex, schema, QStringLiteral("color")));
        const int x0 = static_cast<int>(extractField(paramHex, schema, QStringLiteral("x")));
        const int y0 = static_cast<int>(extractField(paramHex, schema, QStringLiteral("y")));
        const int width = qMax(1, static_cast<int>(extractField(paramHex, schema, QStringLiteral("w"))));
        const int height = qMax(1, static_cast<int>(extractField(paramHex, schema, QStringLiteral("h"))));
        const int addr = static_cast<int>(extractField(paramHex, schema, QStringLiteral("addr")));

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int memIndex = addr + y * width + x;
                if (memIndex < 0 || memIndex >= memory.size())
                    continue;
                blendPixel(frame, x0 + x, y0 + y, color, byteAt(memory, memIndex));
            }
        }
    }
}

void renderText(QImage *frame,
                const FpgaCompiledObject &object,
                const QMap<int, QByteArray> &parameters,
                const QMap<int, SimFont> &fonts,
                const ParamSchema &schema)
{
    if (!frame || schema.isEmpty() || object.paramCount <= 0)
        return;

    const QString paramHex = bytesToHex(parameters.value(object.startParamIndex));
    if (paramHex.isEmpty() || extractField(paramHex, schema, QStringLiteral("enb")) == 0)
        return;

    const int fontIndex = static_cast<int>(extractField(paramHex, schema, QStringLiteral("font_index")));
    if (!fonts.contains(fontIndex))
        return;

    const QList<uint> allCodes = parseCharacterCodes(object.data);
    if (allCodes.isEmpty())
        return;

    const int charOffset = static_cast<int>(extractField(paramHex, schema, QStringLiteral("char_offset")));
    const int charCount = static_cast<int>(extractField(paramHex, schema, QStringLiteral("char_count")));
    if (charOffset < 0 || charOffset >= allCodes.size() || charCount <= 0)
        return;

    const int end = qMin(allCodes.size(), charOffset + charCount);
    const SimFont &font = fonts.value(fontIndex);
    const QColor color = colorFromBgr(extractField(paramHex, schema, QStringLiteral("color")));
    const int originX = static_cast<int>(extractField(paramHex, schema, QStringLiteral("x")));
    const int originY = static_cast<int>(extractField(paramHex, schema, QStringLiteral("y")));

    int penX = 0;
    int minX = 0;
    int top = 0;
    bool firstGlyph = true;
    uint previous = 0;

    for (int i = charOffset; i < end; ++i) {
        const uint code = allCodes[i];
        if (isSpaceCode(code)) {
            penX += font.spaceAdvance;
            previous = code;
            continue;
        }
        const SimGlyph glyph = font.glyphs.value(code);
        if (glyph.width <= 0 || glyph.height <= 0)
            continue;

        if (previous != 0)
            penX += font.kerningPairs.value(kerningKey(previous, code), 0);

        minX = qMin(minX, penX + glyph.bearingX);
        const int glyphTop = glyph.bearingY;
        if (firstGlyph) {
            top = glyphTop;
            firstGlyph = false;
        } else {
            top = qMin(top, glyphTop);
        }
        penX += glyph.advance;
        previous = code;
    }

    penX = 0;
    previous = 0;
    for (int i = charOffset; i < end; ++i) {
        const uint code = allCodes[i];
        if (isSpaceCode(code)) {
            penX += font.spaceAdvance;
            previous = code;
            continue;
        }

        const SimGlyph glyph = font.glyphs.value(code);
        if (glyph.width <= 0 || glyph.height <= 0)
            continue;

        if (previous != 0)
            penX += font.kerningPairs.value(kerningKey(previous, code), 0);

        const int dstX = originX + penX + glyph.bearingX - minX;
        const int dstY = originY + glyph.bearingY - top;
        for (int y = 0; y < glyph.height; ++y) {
            for (int x = 0; x < glyph.width; ++x) {
                const int memIndex = glyph.offset + y * glyph.width + x;
                if (memIndex < 0 || memIndex >= font.memory.size() || memIndex >= glyph.offset + glyph.maskSize)
                    continue;
                blendPixel(frame, dstX + x, dstY + y, color, byteAt(font.memory, memIndex));
            }
        }

        penX += glyph.advance;
        previous = code;
    }
}

void renderRotationObject(QImage *frame,
                          const QString &paramHex,
                          const QMap<int, QByteArray> &memory,
                          const ParamSchema &schema)
{
    if (!frame || schema.isEmpty() || paramHex.isEmpty())
        return;
    if (extractField(paramHex, schema, QStringLiteral("enb")) == 0)
        return;

    const QImage mask = rotationMaskFromMemory(paramHex, schema, memory);
    if (mask.isNull())
        return;

    const int xRot = static_cast<int>(extractField(paramHex, schema, QStringLiteral("xrot")));
    const int yRot = static_cast<int>(extractField(paramHex, schema, QStringLiteral("yrot")));
    const int left = extractSignedField(paramHex, schema, QStringLiteral("left"));
    const int top = extractSignedField(paramHex, schema, QStringLiteral("top"));
    const double sn = extractSignedField(paramHex, schema, QStringLiteral("sin")) / 65536.0;
    const double cs = extractSignedField(paramHex, schema, QStringLiteral("cos")) / 65536.0;
    const double angle = qRadiansToDegrees(qAtan2(sn, cs));

    QPainter painter(frame);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.translate(xRot, yRot);
    painter.rotate(angle);
    painter.drawImage(QPoint(left, top), mask);
}

void renderAviaHorizon(QImage *frame, const QString &paramHex, const ParamSchema &schema)
{
    if (!frame || schema.isEmpty() || extractField(paramHex, schema, QStringLiteral("enb")) == 0)
        return;

    const QColor earth = colorFromBgr(extractField(paramHex, schema, QStringLiteral("earth")));
    const QColor sky = colorFromBgr(extractField(paramHex, schema, QStringLiteral("sky")));
    const QColor line = colorFromBgr(extractField(paramHex, schema, QStringLiteral("hline")));
    const int xo = static_cast<int>(extractField(paramHex, schema, QStringLiteral("xo")));
    const int yo = static_cast<int>(extractField(paramHex, schema, QStringLiteral("yo")));
    const int width = static_cast<int>(extractField(paramHex, schema, QStringLiteral("width"))) + 1;
    const double sn = extractSignedField(paramHex, schema, QStringLiteral("sn")) / 65536.0;
    const double cs = extractSignedField(paramHex, schema, QStringLiteral("cs")) / 65536.0;
    const double angle = qRadiansToDegrees(qAtan2(sn, cs));
    const int span = qMax(frame->width(), frame->height()) * 3;

    QPainter painter(frame);
    painter.setClipRect(frame->rect());
    painter.translate(xo, yo);
    painter.rotate(angle);
    painter.fillRect(QRect(-span, -span, span * 2, span), sky);
    painter.fillRect(QRect(-span, 0, span * 2, span), earth);
    painter.setPen(QPen(line, qMax(1, width)));
    painter.drawLine(QPoint(-span, 0), QPoint(span, 0));
}

void renderRibbonScale(QImage *frame, const QString &paramHex, const ParamSchema &schema)
{
    if (!frame || schema.isEmpty() || extractField(paramHex, schema, QStringLiteral("enable")) == 0)
        return;

    const QColor color = colorFromBgr(extractField(paramHex, schema, QStringLiteral("color")));
    const int left = static_cast<int>(extractField(paramHex, schema, QStringLiteral("left")));
    const int right = static_cast<int>(extractField(paramHex, schema, QStringLiteral("right")));
    const int top = static_cast<int>(extractField(paramHex, schema, QStringLiteral("top")));
    const int bottom = static_cast<int>(extractField(paramHex, schema, QStringLiteral("bottom")));
    const int width = static_cast<int>(extractField(paramHex, schema, QStringLiteral("width"))) + 1;
    const int period = qMax(1, static_cast<int>(extractField(paramHex, schema, QStringLiteral("period"))));
    const int yStart = static_cast<int>(extractField(paramHex, schema, QStringLiteral("ystart")));

    QPainter painter(frame);
    painter.setPen(QPen(color, qMax(1, width)));
    for (int y = yStart; y <= bottom; y += period) {
        if (y >= top)
            painter.drawLine(left, y, right, y);
    }
}

void renderDashedLine(QImage *frame, const QString &paramHex, const ParamSchema &schema)
{
    if (!frame || schema.isEmpty() || extractField(paramHex, schema, QStringLiteral("Enable")) == 0)
        return;

    const QColor color = colorFromBgr(extractField(paramHex, schema, QStringLiteral("Color")));
    const int x0 = static_cast<int>(extractField(paramHex, schema, QStringLiteral("xo")));
    const int y0 = static_cast<int>(extractField(paramHex, schema, QStringLiteral("yo")));
    const int dt = extractSignedField(paramHex, schema, QStringLiteral("dt"));
    const int a = extractSignedField(paramHex, schema, QStringLiteral("a"));
    const int b = extractSignedField(paramHex, schema, QStringLiteral("b"));
    if (dt <= 0)
        return;

    const int step = static_cast<int>(extractField(paramHex, schema, QStringLiteral("StepPow")));
    const int length = static_cast<int>(extractField(paramHex, schema, QStringLiteral("Length")));
    const int phase = static_cast<int>(extractField(paramHex, schema, QStringLiteral("Phase")));
    const int width = static_cast<int>(extractField(paramHex, schema, QStringLiteral("Width")));
    const qint64 wdt = static_cast<qint64>(width + 1) * 128;
    const qint64 qLimit = static_cast<qint64>(dt) << 8;
    const int dashMask = (1 << (qBound(0, step, 7) + 1)) - 1;

    for (int y = 0; y < frame->height(); ++y) {
        for (int x = 0; x < frame->width(); ++x) {
            const qint64 difx = x - x0;
            const qint64 dify = y - y0;
            const qint64 q = static_cast<qint64>(a) * difx + static_cast<qint64>(b) * dify;
            if (q < 0 || q > qLimit)
                continue;

            const bool dashGate = (((q + (static_cast<qint64>(phase) << 8)) >> 8) & dashMask) <= length;
            if (!dashGate)
                continue;

            const qint64 t = static_cast<qint64>(b) * difx - static_cast<qint64>(a) * dify;
            const qint64 val = wdt - qAbs(t);
            const int mask = val < 0 ? 0 : (val >= 256 ? 256 : static_cast<int>(val) & 0x1FF);
            if (mask > 0)
                blendPixel256(frame, x, y, color, mask);
        }
    }
}
}

void FpgaSimulator::loadBundle(const FpgaPacketBundle &bundle)
{
    m_bundle = bundle;
    decodePackets();
}

void FpgaSimulator::decodePackets()
{
    m_parameters.clear();
    m_staticMemory.clear();
    m_rotationMemory.clear();
    m_warnings = m_bundle.warnings;

    for (const FpgaPacket &packet : m_bundle.packets) {
        const QByteArray &bytes = packet.bytes;
        if (bytes.size() < 11 || bytes.mid(0, 4) != QByteArray::fromHex("5341494E")) {
            m_warnings.append(QStringLiteral("Skipped malformed packet."));
            continue;
        }

        const quint8 type = byteAt(bytes, 6);
        if (type == static_cast<quint8>(FpgaPacketType::Parameter)) {
            if (bytes.size() < 11)
                continue;
            const int objId = readUint16(bytes, 7);
            const int length = readUint16(bytes, 9);
            m_parameters.insert(objId, bytes.mid(11, length));
        } else if (type == static_cast<quint8>(FpgaPacketType::StaticMemory)
                   || type == static_cast<quint8>(FpgaPacketType::RotationMemory)) {
            if (bytes.size() < 12)
                continue;
            const int memId = byteAt(bytes, 7);
            const int address = readUint16(bytes, 8);
            const int length = readUint16(bytes, 10);
            const QByteArray payload = bytes.mid(12, length);
            if (type == static_cast<quint8>(FpgaPacketType::RotationMemory)) {
                m_rotationMemory[memId].insert(address, payload);
            } else {
                QByteArray &memory = m_staticMemory[memId];
                if (memory.size() < address + payload.size())
                    memory.resize(address + payload.size());
                std::copy(payload.constBegin(), payload.constEnd(), memory.begin() + address);
            }
        }
    }
}

QImage FpgaSimulator::renderFrame() const
{
    const FpgaCompiledDocument &document = m_bundle.document;
    QImage frame(qMax(1, document.width), qMax(1, document.height), QImage::Format_ARGB32);
    frame.fill(document.backgroundColor);
    const QMap<int, SimFont> fonts = buildFonts(document, m_parameters, m_staticMemory);

    for (const FpgaCompiledObject &object : document.objects) {
        const QString paramHex = bytesToHex(m_parameters.value(object.startParamIndex));
        if (object.type == QStringLiteral("rectangle")) {
            renderRectangle(&frame, paramHex, document.schemas.value(QStringLiteral("rectangle")), false);
        } else if (object.type == QStringLiteral("rectangle_a")) {
            renderRectangle(&frame, paramHex, document.schemas.value(QStringLiteral("rectangle_a")), true);
        } else if (object.type == QStringLiteral("staticgroup")) {
            renderStaticGroup(&frame, object, m_parameters, m_staticMemory.value(object.memId), document.schemas.value(QStringLiteral("staticgroup")));
        } else if (object.type == QStringLiteral("text")) {
            renderText(&frame, object, m_parameters, fonts, document.schemas.value(QStringLiteral("text")));
        } else if (object.type == QStringLiteral("rotationobject")) {
            renderRotationObject(&frame, paramHex, m_rotationMemory.value(object.memId), document.schemas.value(QStringLiteral("rotationobject")));
        } else if (object.type == QStringLiteral("aviagorizont") || object.type == QStringLiteral("aviahorizont")) {
            renderAviaHorizon(&frame, paramHex, document.schemas.value(QStringLiteral("aviagorizont")));
        } else if (object.type == QStringLiteral("dashed_line")) {
            renderDashedLine(&frame, paramHex, document.schemas.value(QStringLiteral("dashed_line")));
        } else if (object.type == QStringLiteral("RibonScale") || object.type == QStringLiteral("ribonscale")) {
            renderRibbonScale(&frame, paramHex, document.schemas.value(QStringLiteral("RibonScale")));
        }
    }

    return frame;
}
