// FontAtlasXml - чтение и проверка шрифтового атласа из XML.

#include "text/FontAtlasXml.h"

#include "BitParser.h"

#include <QDomElement>
#include <QRegularExpression>

bool readFontAtlas(const QDomElement &element, const ParamSchema &schema,
                   FpgaFont *font, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };
    FpgaFont atlas;
    bool ok = false;
    atlas.index = element.attribute("index").toInt(&ok);
    if (!ok || atlas.index < 0 || atlas.index > 255)
        return fail(QStringLiteral("Индекс ресурса <font> должен быть в диапазоне 0…255."));
    atlas.name = element.attribute("name");
    atlas.size = element.attribute("size").toInt(&ok);
    if (!ok || atlas.size <= 0)
        return fail(QStringLiteral("У ресурса <font> некорректный размер шрифта."));

    const auto values = element.firstChildElement("data").text().split(
        QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
    for (const auto &value : values) {
        if (value.size() != 1 || value[0] < QLatin1Char('0') || value[0] > QLatin1Char('7'))
            return fail(QStringLiteral("Растр шрифта должен содержать только уровни маски 0…7."));
        atlas.maskData.append(value);
    }
    atlas.volume = atlas.maskData.size() * 3;

    for (auto init = element.firstChildElement("init"); !init.isNull(); init = init.nextSiblingElement("init")) {
        const QString hex = init.text().trimmed();
        auto field = [&schema, &hex](const QString &name) {
            const auto info = schema.value(name);
            return int(BitParser::extract(hex, info.offset, info.size));
        };
        auto signedField = [&schema, &hex](const QString &name) {
            const auto info = schema.value(name);
            return BitParser::extractSigned(hex, info.offset, info.size);
        };
        FpgaGlyph glyph;
        glyph.enabled = field("enb") != 0;
        glyph.code = field("code");
        glyph.literal = QChar(static_cast<ushort>(glyph.code));
        if (init.attribute("literal") != QString(glyph.literal) || atlas.glyphs.contains(glyph.literal))
            return fail(QStringLiteral("Повторный символ или literal не соответствует коду глифа U+%1.")
                .arg(QString::number(glyph.code, 16).toUpper()));
        if (field("font_index") != atlas.index)
            return fail(QStringLiteral("Индекс глифа не соответствует индексу ресурса <font>."));
        glyph.width = field("w");
        glyph.height = field("h");
        glyph.advance = field("advance");
        glyph.bearingX = signedField("bearing_x");
        glyph.bearingY = signedField("bearing_y");
        glyph.ascent = field("ascent");
        glyph.descent = field("descent");
        glyph.offset = field("offset");
        glyph.size = atlas.size;
        const qint64 area = qint64(glyph.width) * glyph.height;
        if (area <= 0 || field("mask_size") != area)
            return fail(QStringLiteral("Некорректный mask_size символа U+%1: должен равняться w × h и помещаться в 12 бит.")
                .arg(QString::number(glyph.code, 16).toUpper()));
        if (glyph.offset + area > atlas.maskData.size())
            return fail(QStringLiteral("Растр символа U+%1 выходит за границы <data> шрифта.")
                .arg(QString::number(glyph.code, 16).toUpper()));
        QStringList rows;
        for (int y = 0; y < glyph.height; ++y)
            rows.append(atlas.maskData.mid(glyph.offset + y * glyph.width, glyph.width));
        glyph.maskRows = rows.join('\n');
        atlas.glyphOrder.append(glyph.literal);
        atlas.glyphs.insert(glyph.literal, glyph);
    }
    for (auto pair = element.firstChildElement("kerning"); !pair.isNull(); pair = pair.nextSiblingElement("kerning")) {
        const QString left = pair.attribute("left");
        const QString right = pair.attribute("right");
        const int delta = pair.attribute("delta").toInt(&ok);
        if (!ok || left.size() != 1 || right.size() != 1
            || !atlas.glyphs.contains(left[0]) || !atlas.glyphs.contains(right[0]))
            return fail(QStringLiteral("Некорректная пара кернинга в ресурсе <font>."));
        if (atlas.kerningPairs.contains(left + right))
            return fail(QStringLiteral("Повторная пара кернинга в ресурсе <font>."));
        atlas.kerningPairs.insert(left + right, delta);
    }
    *font = atlas;
    return true;
}
