#include <QFile>
#include <QDomDocument>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QSet>
#include <QStringEncoder>
#include <QImageReader>
#include <QSvgRenderer>
#include <QSettings>

#include "ProjectManager.h"
#include "EditorProjectDocument.h"
#include "FpgaSchemaRegistry.h"
#include "ObjectsManager.h"
#include "RectangleObject.h"
#include "DashedLineObject.h"
#include "RibbonScaleObject.h"
#include "RotationObject.h"
#include "StaticGroupObject.h"
#include "ImageObject.h"
#include "AviaHorizonObject.h"
#include "TextObject.h"
#include "BitParser.h"
#include "DebugDumper.h"

#include <algorithm>

namespace {
int schemaBitLength(const ParamSchema &schema)
{
    int maxBit = 0;
    for (auto it = schema.constBegin(); it != schema.constEnd(); ++it) {
        maxBit = qMax(maxBit, it.value().offset + it.value().size);
    }
    return maxBit;
}

QString buildInitHex(const ParamSchema &schema, const QMap<QString, quint32> &params)
{
    const int hexLength = (schemaBitLength(schema) + 3) / 4;
    QString hexStr(hexLength, QLatin1Char('0'));

    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (schema.contains(it.key())) {
            const auto &info = schema[it.key()];
            BitParser::inject(hexStr, info.offset, info.size, it.value());
        }
    }

    return hexStr;
}

QString serializeMaskData(const QImage &image)
{
    QStringList values;
    values.reserve(image.width() * image.height());

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = image.pixelColor(x, y).alpha();
            const int level = qBound(0, qRound(alpha * 7.0 / 255.0), 7);
            values.append(QString::number(level));
        }
    }

    return values.join(", ");
}

QString formatProjectColor(const QColor &color)
{
    const uint bgVal = (color.blue() << 16) | (color.green() << 8) | color.red();
    return QString("#%1").arg(bgVal, 0, 16);
}

QString serializeStaticGroupData(const StaticGroupObject *group)
{
    if (!group || group->states.isEmpty()) {
        return QStringLiteral("7");
    }

    int totalSize = 1;
    for (int i = 0; i < group->states.size(); ++i) {
        const GroupState &state = group->states[i];
        totalSize = qMax(totalSize, state.addr + qMax(1, state.w * state.h));
    }

    QVector<int> values(totalSize, 0);

    for (int i = 0; i < group->states.size(); ++i) {
        const GroupState &state = group->states[i];
        QImage image;
        if (i < group->maskImages.size() && !group->maskImages[i].isNull()) {
            image = group->maskImages[i];
        } else {
            image = QImage(qMax(1, state.w), qMax(1, state.h), QImage::Format_ARGB32);
            image.fill(QColor(255, 255, 255, 255));
        }

        const int width = qMin(image.width(), qMax(1, state.w));
        const int height = qMin(image.height(), qMax(1, state.h));
        int idx = qMax(0, state.addr);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (idx >= values.size())
                    break;

                const int alpha = image.pixelColor(x, y).alpha();
                values[idx++] = qBound(0, qRound(alpha * 7.0 / 255.0), 7);
            }
        }
    }

    QStringList parts;
    parts.reserve(values.size());
    for (int value : values) {
        parts.append(QString::number(value));
    }
    return parts.join(", ");
}

QDomElement createObjectElement(QDomDocument &doc, const QString &tagName, const ParamSchema &schema, const QSharedPointer<BaseObject> &obj)
{
    QDomElement objEl = doc.createElement(tagName);

    if (auto staticGroup = dynamic_cast<StaticGroupObject*>(obj.data())) {
        objEl.setAttribute("nomber", staticGroup->groupNumber > 0 ? staticGroup->groupNumber : staticGroup->states.size());

        const int stateCount = qMax(1, staticGroup->states.size());
        for (int stateIndex = 0; stateIndex < stateCount; ++stateIndex) {
            QDomElement initEl = doc.createElement("init");
            initEl.setAttribute("index", stateIndex + 1);
            initEl.appendChild(doc.createTextNode(buildInitHex(schema, staticGroup->serializeState(stateIndex))));
            objEl.appendChild(initEl);
        }

        QDomElement dataEl = doc.createElement("data");
        dataEl.appendChild(doc.createTextNode(serializeStaticGroupData(staticGroup)));
        objEl.appendChild(dataEl);
        return objEl;
    }

    QDomElement initEl = doc.createElement("init");
    initEl.appendChild(doc.createTextNode(buildInitHex(schema, obj->serializeParams())));
    objEl.appendChild(initEl);

    if (auto rotation = dynamic_cast<RotationObject*>(obj.data())) {
        QDomElement dataEl = doc.createElement("data");
        QImage image = rotation->maskImage;
        if (image.isNull()) {
            image = QImage(1, 1, QImage::Format_ARGB32);
            image.fill(QColor(255, 255, 255, 255));
        }
        dataEl.setAttribute("width", image.width());
        dataEl.setAttribute("height", image.height());
        dataEl.appendChild(doc.createTextNode(serializeMaskData(image)));
        objEl.appendChild(dataEl);
    }

    return objEl;
}

QString maskRowsFromImage(const QImage &image)
{
    QStringList rows;
    rows.reserve(image.height());
    for (int y = 0; y < image.height(); ++y) {
        QString row;
        row.reserve(image.width());
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = image.pixelColor(x, y).alpha();
            row.append(QString::number(qBound(0, qRound(alpha * 7.0 / 255.0), 7)));
        }
        rows.append(row);
    }
    return rows.join('\n');
}

QString alphabetCharacters(const QSet<QString> &groups)
{
    QString characters;
    auto appendUnique = [&characters](const QString &value) {
        for (const QChar ch : value) {
            if (!characters.contains(ch))
                characters.append(ch);
        }
    };

    if (groups.contains(QStringLiteral("digits")))
        appendUnique(QStringLiteral("0123456789"));
    if (groups.contains(QStringLiteral("latin_upper")))
        appendUnique(QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    if (groups.contains(QStringLiteral("latin_lower")))
        appendUnique(QStringLiteral("abcdefghijklmnopqrstuvwxyz"));
    if (groups.contains(QStringLiteral("cyrillic_upper")))
        appendUnique(QStringLiteral("АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"));
    if (groups.contains(QStringLiteral("cyrillic_lower")))
        appendUnique(QStringLiteral("абвгдеёжзийклмнопрстуфхцчшщъыьэюя"));

    return characters;
}

QImage renderGlyphMask(const QFont &font, const QChar ch, FpgaGlyph *glyph)
{
    QFontMetrics metrics(font);
    QRect tight = metrics.tightBoundingRect(QString(ch));
    if (tight.isEmpty())
        tight = QRect(0, -metrics.ascent(), qMax(1, metrics.horizontalAdvance(ch)), metrics.height());

    const int padding = qMax(2, (qMax(1, font.pixelSize()) + 7) / 8);
    QImage image(qMax(1, tight.width() + padding * 2), qMax(1, tight.height() + padding * 2), QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QPoint(padding - tight.left(), padding - tight.top()), QString(ch));
    painter.end();

    if (glyph) {
        glyph->literal = ch;
        glyph->code = ch.unicode();
        glyph->width = image.width();
        glyph->height = image.height();
        glyph->advance = metrics.horizontalAdvance(ch);
        glyph->bearingX = tight.left();
        glyph->bearingY = tight.top();
        glyph->ascent = metrics.ascent();
        glyph->descent = metrics.descent();
        glyph->floater = tight.top();
        glyph->size = font.pixelSize();
        glyph->maskRows = maskRowsFromImage(image);
        glyph->maskImage = image;
    }

    return image;
}

QStringList maskRowsToValues(const QString &rows)
{
    QStringList values;
    const QStringList lines = rows.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        for (const QChar ch : trimmed) {
            if (ch.isDigit())
                values.append(QString(ch));
        }
    }
    return values;
}

QDomElement createSchemaElement(QDomDocument &doc, const QString &schemaName)
{
    QDomElement schemaEl = doc.createElement(schemaName);
    const auto fields = FpgaSchemaRegistry::instance()->fieldsForSchema(schemaName);
    for (const auto &field : fields) {
        QDomElement fieldEl = doc.createElement(field.name);
        fieldEl.setAttribute("offset", field.offset);
        fieldEl.setAttribute("size", field.size);
        schemaEl.appendChild(fieldEl);
    }
    return schemaEl;
}

void appendParameterSchemas(QDomDocument &doc, QDomElement &root, const QSet<QString> &schemaNames)
{
    QDomElement paramsEl = doc.createElement("parameters");
    root.appendChild(paramsEl);

    const QStringList orderedSchemas = FpgaSchemaRegistry::instance()->orderedSchemaNames();
    for (const QString &schemaName : orderedSchemas) {
        if (schemaNames.contains(schemaName))
            paramsEl.appendChild(createSchemaElement(doc, schemaName));
    }
}

QDomElement createImageObjectElement(QDomDocument &doc, const ImageObject *image)
{
    QDomElement imageEl = doc.createElement("image");
    imageEl.setAttribute("x", qRound(image->x));
    imageEl.setAttribute("y", qRound(image->y));
    imageEl.setAttribute("width", qRound(image->width));
    imageEl.setAttribute("height", qRound(image->height));
    imageEl.setAttribute("format", image->format);
    imageEl.setAttribute("source_name", image->sourceName);
    imageEl.setAttribute("mask_color", image->maskColor.name());
    imageEl.setAttribute("mask_color_auto", image->autoMaskColor ? 1 : 0);
    imageEl.setAttribute("visible", image->isViewVisible() ? 1 : 0);
    imageEl.setAttribute("export", image->isExportEnabled() ? 1 : 0);

    QDomElement sourceEl = doc.createElement("source");
    sourceEl.setAttribute("encoding", "base64");
    sourceEl.appendChild(doc.createTextNode(QString::fromLatin1(image->sourcePayload().toBase64())));
    imageEl.appendChild(sourceEl);
    return imageEl;
}

QDomElement createEditableTextElement(QDomDocument &doc, const TextObject *text)
{
    const GroupState state = text->states.isEmpty() ? GroupState{} : text->states.first();
    QDomElement textEl = doc.createElement("textobject");
    textEl.setAttribute("text", text->text);
    textEl.setAttribute("x", state.x);
    textEl.setAttribute("y", state.y);
    textEl.setAttribute("font_family", text->fontFamily);
    textEl.setAttribute("font_size", text->pixelSize);
    textEl.setAttribute("font_index", text->fontIndex);
    textEl.setAttribute("color", state.color.name());
    textEl.setAttribute("visible", text->isViewVisible() ? 1 : 0);
    textEl.setAttribute("export", text->isExportEnabled() ? 1 : 0);
    return textEl;
}

QDomElement createStaticGroupElementFromImage(QDomDocument &doc, const ParamSchema &schema, const ImageObject *image)
{
    StaticGroupObject group;
    GroupState state;
    state.x = qRound(image->x);
    state.y = qRound(image->y);
    state.w = qMax(1, qRound(image->width));
    state.h = qMax(1, qRound(image->height));
    state.addr = 0;
    state.color = image->effectiveMaskColor();
    state.enabled = image->isViewVisible();
    group.groupNumber = 1;
    group.states = {state};
    group.maskImages = {image->renderedImage()};
    return createObjectElement(doc, QStringLiteral("staticgroup"), schema, QSharedPointer<BaseObject>(&group, [](BaseObject*){}));
}

struct ExportFontKey
{
    QString family;
    int size = 14;

    bool operator<(const ExportFontKey &other) const
    {
        if (family != other.family)
            return family < other.family;
        return size < other.size;
    }
};

QDomElement createFontResourceElement(QDomDocument &doc,
                                      const ExportFontKey &key,
                                      int fontIndex,
                                      const QString &characters,
                                      const ParamSchema &fontSchema,
                                      FpgaFont *fontOut)
{
    FpgaFont font;
    font.index = fontIndex;
    font.name = key.family;
    font.size = key.size;

    QDomElement fontEl = doc.createElement("font");
    fontEl.setAttribute("index", fontIndex);
    fontEl.setAttribute("name", key.family);
    fontEl.setAttribute("size", key.size);
    fontEl.setAttribute("count", characters.size());

    QFont qfont(key.family);
    qfont.setPixelSize(key.size);

    QStringList dataValues;
    int offset = 0;
    int glyphIndex = 0;
    for (const QChar ch : characters) {
        FpgaGlyph glyph;
        renderGlyphMask(qfont, ch, &glyph);
        glyph.offset = offset;
        glyph.size = key.size;
        glyphIndex++;

        const QStringList glyphValues = maskRowsToValues(glyph.maskRows);
        offset += glyphValues.size();
        dataValues.append(glyphValues);
        font.glyphs.insert(ch, glyph);

        QDomElement initEl = doc.createElement("init");
        initEl.setAttribute("index", glyphIndex);
        initEl.setAttribute("literal", QString(ch));
        initEl.setAttribute("code", glyph.code);
        initEl.appendChild(doc.createTextNode(buildInitHex(fontSchema, {
            {"enb", 1},
            {"code", static_cast<quint32>(glyph.code)},
            {"w", static_cast<quint32>(glyph.width)},
            {"h", static_cast<quint32>(glyph.height)},
            {"advance", static_cast<quint32>(glyph.advance)},
            {"bearing_x", static_cast<quint32>(static_cast<qint32>(glyph.bearingX))},
            {"bearing_y", static_cast<quint32>(static_cast<qint32>(glyph.bearingY))},
            {"ascent", static_cast<quint32>(glyph.ascent)},
            {"descent", static_cast<quint32>(glyph.descent)},
            {"offset", static_cast<quint32>(glyph.offset)},
            {"mask_size", static_cast<quint32>(glyphValues.size())},
            {"font_index", static_cast<quint32>(fontIndex)}
        })));
        fontEl.appendChild(initEl);
    }

    font.volume = dataValues.size() * 3;
    fontEl.setAttribute("volume", font.volume);

    QFontMetrics metrics(qfont);
    for (const QChar left : characters) {
        for (const QChar right : characters) {
            const QString pair = QString(left) + QString(right);
            const int delta = metrics.horizontalAdvance(pair)
                - metrics.horizontalAdvance(left)
                - metrics.horizontalAdvance(right);
            if (delta == 0)
                continue;

            font.kerningPairs.insert(pair, delta);
            QDomElement kerningEl = doc.createElement("kerning");
            kerningEl.setAttribute("left", QString(left));
            kerningEl.setAttribute("right", QString(right));
            kerningEl.setAttribute("delta", delta);
            fontEl.appendChild(kerningEl);
        }
    }

    QDomElement dataEl = doc.createElement("data");
    dataEl.appendChild(doc.createTextNode(dataValues.join(", ")));
    fontEl.appendChild(dataEl);

    if (fontOut)
        *fontOut = font;

    return fontEl;
}

FpgaFont parseFontElement(const QDomElement &fontEl)
{
    FpgaFont font;
    font.index = fontEl.attribute("index", "0").toInt();
    font.name = fontEl.attribute("name", "Arial");
    font.size = fontEl.attribute("size", "14").toInt();
    font.volume = fontEl.attribute("volume", "0").toInt();

    QStringList dataValues;
    const QString dataText = fontEl.firstChildElement("data").text();
    for (const QString &part : dataText.split(',', Qt::SkipEmptyParts))
        dataValues.append(part.trimmed());

    const ParamSchema fontSchema = FpgaSchemaRegistry::instance()->buildSchema(QStringLiteral("font"));
    QDomElement glyphEl = fontEl.firstChildElement();
    while (!glyphEl.isNull()) {
        if (glyphEl.tagName() == QStringLiteral("init")) {
            const QString literal = glyphEl.attribute("literal");
            const QString hex = glyphEl.text().trimmed();
            if (!literal.isEmpty() && !hex.isEmpty()) {
                FpgaGlyph glyph;
                glyph.literal = literal.front();
                glyph.code = BitParser::extract(hex, fontSchema["code"].offset, fontSchema["code"].size);
                glyph.width = BitParser::extract(hex, fontSchema["w"].offset, fontSchema["w"].size);
                glyph.height = BitParser::extract(hex, fontSchema["h"].offset, fontSchema["h"].size);
                glyph.advance = BitParser::extract(hex, fontSchema["advance"].offset, fontSchema["advance"].size);
                glyph.bearingX = BitParser::extractSigned(hex, fontSchema["bearing_x"].offset, fontSchema["bearing_x"].size);
                glyph.bearingY = BitParser::extractSigned(hex, fontSchema["bearing_y"].offset, fontSchema["bearing_y"].size);
                glyph.ascent = BitParser::extract(hex, fontSchema["ascent"].offset, fontSchema["ascent"].size);
                glyph.descent = BitParser::extract(hex, fontSchema["descent"].offset, fontSchema["descent"].size);
                glyph.offset = BitParser::extract(hex, fontSchema["offset"].offset, fontSchema["offset"].size);
                glyph.size = font.size;
                glyph.floater = glyph.bearingY;

                QStringList rows;
                int idx = glyph.offset;
                for (int y = 0; y < glyph.height; ++y) {
                    QString row;
                    row.reserve(glyph.width);
                    for (int x = 0; x < glyph.width; ++x) {
                        row.append(idx < dataValues.size() ? dataValues[idx] : QStringLiteral("0"));
                        ++idx;
                    }
                    rows.append(row);
                }
                glyph.maskRows = rows.join('\n');
                font.glyphs.insert(glyph.literal, glyph);
            }
        } else if (glyphEl.tagName() == QStringLiteral("kerning")) {
            const QString left = glyphEl.attribute("left");
            const QString right = glyphEl.attribute("right");
            if (!left.isEmpty() && !right.isEmpty())
                font.kerningPairs.insert(left.left(1) + right.left(1), glyphEl.attribute("delta", "0").toInt());
        } else if (glyphEl.tagName() == QStringLiteral("digit") || glyphEl.tagName() == QStringLiteral("upper")) {
            const QString literal = glyphEl.attribute("literal");
            if (!literal.isEmpty()) {
                FpgaGlyph glyph;
                glyph.literal = literal.front();
                glyph.code = glyphEl.attribute("code", QString::number(glyph.literal.unicode())).toInt();
                glyph.width = glyphEl.attribute("width", "0").toInt();
                glyph.height = glyphEl.attribute("height", "0").toInt();
                glyph.floater = glyphEl.attribute("floater", "0").toInt();
                glyph.offset = glyphEl.attribute("offset", "0").toInt();
                glyph.size = font.size;
                const QStringList rows = glyphEl.text().split('\n', Qt::SkipEmptyParts);
                QStringList trimmedRows;
                for (const QString &row : rows) {
                    const QString trimmed = row.trimmed();
                    if (!trimmed.isEmpty())
                        trimmedRows.append(trimmed);
                }
                glyph.maskRows = trimmedRows.join('\n');
                font.glyphs.insert(glyph.literal, glyph);
            }
        }
        glyphEl = glyphEl.nextSiblingElement();
    }

    return font;
}

QDomDocument buildEditableXmlDocument(const QString &projectName,
                                      int canvasWidth,
                                      int canvasHeight,
                                      const QColor &backgroundColor,
                                      bool showGrid,
                                      bool snapToCanvas,
                                      bool snapToGrid,
                                      bool snapToObjects,
                                      const QMap<QString, ParamSchema> &schemas,
                                      const QMap<QString, QString> &schemaAliases,
                                      const QList<QSharedPointer<BaseObject>> &objects,
                                      const QStringList &objectTags)
{
    QDomDocument doc;
    doc.appendChild(doc.createProcessingInstruction("xml", "version='1.0' encoding='windows-1251'"));

    QDomElement root = doc.createElement("project");
    root.setAttribute("name", projectName.isEmpty() ? QStringLiteral("Untitled") : projectName);
    root.setAttribute("width", canvasWidth);
    root.setAttribute("height", canvasHeight);
    root.setAttribute("bgcolor", formatProjectColor(backgroundColor));
    root.setAttribute("mode", "editable");
    root.setAttribute("grid", showGrid ? 1 : 0);
    root.setAttribute("snap_screen", snapToCanvas ? 1 : 0);
    root.setAttribute("snap_grid", snapToGrid ? 1 : 0);
    root.setAttribute("snap_objects", snapToObjects ? 1 : 0);
    doc.appendChild(root);

    QSet<QString> schemasToWrite;
    for (const QString &schemaName : FpgaSchemaRegistry::instance()->defaultSchemaNames())
        schemasToWrite.insert(schemaName);
    appendParameterSchemas(doc, root, schemasToWrite);

    QDomElement objectsEl = doc.createElement("objects");
    root.appendChild(objectsEl);
    auto *registry = FpgaSchemaRegistry::instance();

    for (int objIdx = 0; objIdx < objects.size(); ++objIdx) {
        const auto &obj = objects[objIdx];
        if (auto image = dynamic_cast<ImageObject*>(obj.data())) {
            objectsEl.appendChild(createImageObjectElement(doc, image));
            continue;
        }
        if (auto text = dynamic_cast<TextObject*>(obj.data())) {
            objectsEl.appendChild(createEditableTextElement(doc, text));
            continue;
        }

        const QString tagName = objIdx < objectTags.size() ? objectTags[objIdx] : QString();
        const QString schemaName = schemaAliases.value(tagName, registry->canonicalSchemaName(tagName));
        if (tagName.isEmpty() || !schemas.contains(schemaName))
            continue;

        QDomElement element = createObjectElement(doc, tagName, schemas[schemaName], obj);
        element.setAttribute("visible", obj->isViewVisible() ? 1 : 0);
        element.setAttribute("export", obj->isExportEnabled() ? 1 : 0);
        objectsEl.appendChild(element);
    }

    return doc;
}

QDomDocument buildCompiledXmlDocument(const QString &projectName,
                                      int canvasWidth,
                                      int canvasHeight,
                                      const QColor &backgroundColor,
                                      bool showGrid,
                                      bool snapToCanvas,
                                      bool snapToGrid,
                                      bool snapToObjects,
                                      const QMap<QString, ParamSchema> &schemas,
                                      const QMap<QString, QString> &schemaAliases,
                                      const QList<QSharedPointer<BaseObject>> &objects,
                                      const QStringList &objectTags,
                                      const QSet<QString> &alphabetGroups,
                                      QMap<int, FpgaFont> *fontsOut)
{
    QDomDocument doc;
    doc.appendChild(doc.createProcessingInstruction("xml", "version='1.0' encoding='windows-1251'"));

    QDomElement root = doc.createElement("project");
    root.setAttribute("name", projectName.isEmpty() ? QStringLiteral("Untitled") : projectName);
    root.setAttribute("width", canvasWidth);
    root.setAttribute("height", canvasHeight);
    root.setAttribute("bgcolor", formatProjectColor(backgroundColor));
    root.setAttribute("mode", "compiled");
    root.setAttribute("grid", showGrid ? 1 : 0);
    root.setAttribute("snap_screen", snapToCanvas ? 1 : 0);
    root.setAttribute("snap_grid", snapToGrid ? 1 : 0);
    root.setAttribute("snap_objects", snapToObjects ? 1 : 0);
    doc.appendChild(root);

    QSet<QString> schemasToWrite;
    for (const QString &schemaName : FpgaSchemaRegistry::instance()->defaultSchemaNames())
        schemasToWrite.insert(schemaName);
    appendParameterSchemas(doc, root, schemasToWrite);

    QDomElement objectsEl = doc.createElement("objects");
    root.appendChild(objectsEl);

    auto *registry = FpgaSchemaRegistry::instance();
    const ParamSchema staticSchema = schemas.value(QStringLiteral("staticgroup"), registry->buildSchema(QStringLiteral("staticgroup")));
    const ParamSchema fontSchema = registry->buildSchema(QStringLiteral("font"));
    const ParamSchema textSchema = registry->buildSchema(QStringLiteral("text"));

    QMap<ExportFontKey, QList<TextObject*>> textByFont;
    for (const auto &obj : objects) {
        if (auto text = dynamic_cast<TextObject*>(obj.data())) {
            if (text->isExportEnabled())
                textByFont[{text->fontFamily, text->pixelSize}].append(text);
        }
    }

    const QString baseCharacters = alphabetCharacters(alphabetGroups);
    QMap<ExportFontKey, int> fontIndexByKey;
    QMap<int, FpgaFont> generatedFonts;
    int nextFontIndex = 0;
    for (auto it = textByFont.constBegin(); it != textByFont.constEnd(); ++it) {
        QString characters = baseCharacters;
        for (TextObject *text : it.value()) {
            if (text->hasFontAtlas()) {
                for (auto glyphIt = text->fontAtlas().glyphs.constBegin(); glyphIt != text->fontAtlas().glyphs.constEnd(); ++glyphIt) {
                    if (!characters.contains(glyphIt.key()))
                        characters.append(glyphIt.key());
                }
            }
            for (const QChar ch : text->text) {
                if (!ch.isSpace() && !characters.contains(ch))
                    characters.append(ch);
            }
        }
        if (characters.isEmpty())
            continue;

        FpgaFont font;
        objectsEl.appendChild(createFontResourceElement(doc, it.key(), nextFontIndex, characters, fontSchema, &font));
        fontIndexByKey.insert(it.key(), nextFontIndex);
        generatedFonts.insert(nextFontIndex, font);
        ++nextFontIndex;
    }

    QDomElement textEl = doc.createElement("text");
    QStringList textDataCodes;
    int textLineIndex = 0;
    bool textElementAppended = false;

    for (int objIdx = 0; objIdx < objects.size(); ++objIdx) {
        const auto &obj = objects[objIdx];
        if (!obj->isExportEnabled())
            continue;

        if (auto image = dynamic_cast<ImageObject*>(obj.data())) {
            objectsEl.appendChild(createStaticGroupElementFromImage(doc, staticSchema, image));
            continue;
        }

        if (auto text = dynamic_cast<TextObject*>(obj.data())) {
            if (!textElementAppended) {
                objectsEl.appendChild(textEl);
                textElementAppended = true;
            }

            const ExportFontKey key{text->fontFamily, text->pixelSize};
            const int fontIndex = fontIndexByKey.value(key, 0);
            const GroupState state = text->states.isEmpty() ? GroupState{} : text->states.first();
            const int charOffset = textDataCodes.size();
            for (const QChar ch : text->text)
                textDataCodes.append(QString::number(ch.unicode()));

            QDomElement initEl = doc.createElement("init");
            initEl.setAttribute("index", textLineIndex);
            initEl.setAttribute("text", text->text);
            initEl.appendChild(doc.createTextNode(buildInitHex(textSchema, {
                {"enb", text->isViewVisible() ? 1u : 0u},
                {"color", BitParser::colorToBgr(state.color)},
                {"x", static_cast<quint32>(state.x)},
                {"y", static_cast<quint32>(state.y)},
                {"font_index", static_cast<quint32>(fontIndex)},
                {"char_offset", static_cast<quint32>(charOffset)},
                {"char_count", static_cast<quint32>(text->text.size())}
            })));
            textEl.appendChild(initEl);
            ++textLineIndex;
            continue;
        }

        const QString tagName = objIdx < objectTags.size() ? objectTags[objIdx] : QString();
        const QString schemaName = schemaAliases.value(tagName, registry->canonicalSchemaName(tagName));
        if (tagName.isEmpty() || !schemas.contains(schemaName))
            continue;
        objectsEl.appendChild(createObjectElement(doc, tagName, schemas[schemaName], obj));
    }

    if (textLineIndex > 0) {
        textEl.setAttribute("count", textLineIndex);
        QDomElement dataEl = doc.createElement("data");
        dataEl.appendChild(doc.createTextNode(textDataCodes.join(", ")));
        textEl.appendChild(dataEl);
    }

    if (fontsOut)
        *fontsOut = generatedFonts;

    return doc;
}
}

ProjectManager::ProjectManager() : m_document(new EditorProjectDocument())
{
    reloadGlobalSettings();
}

ProjectManager* ProjectManager::instance()
{
    static ProjectManager s_instance;
    return &s_instance;
}

bool ProjectManager::loadFromFile(const QString &fileName)
{
    return loadXmlProject(fileName);
}

bool ProjectManager::loadXmlProject(const QString &fileName)
{
    auto *registry = FpgaSchemaRegistry::instance();
    m_filePath = fileName;
    m_objects.clear();
    m_objectTags.clear();
    m_fonts.clear();
    
    emit logMessage(tr("Загрузка: %1").arg(fileName));
    
    //открываем файл
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage(tr("ОШИБКА: Не удалось открыть файл"));
        return false;
    }
    
    //парсим XML
    QDomDocument doc;
    QDomDocument::ParseResult result = doc.setContent(&file);
    if (!result) {
        emit logMessage(tr("ОШИБКА XML: %1").arg(result.errorMessage));
        file.close();
        return false;
    }
    file.close();
    
    //получаем корневой элемент
    QDomElement root = doc.documentElement();
    m_editMode = root.attribute("mode") == QStringLiteral("editable")
        ? ProjectEditMode::EditableXml
        : ProjectEditMode::RestrictedFpgaXml;
    
    //читаем метаданные проекта
    m_document->setProjectName(root.attribute("name", "Untitled"));
    m_document->setCanvasSize(root.attribute("width", "640").toInt(), root.attribute("height", "480").toInt());
    m_document->setBackgroundColor(Qt::black);
    m_showGrid = root.attribute("grid", "0").toInt() != 0;
    m_snapToCanvas = root.attribute("snap_screen", "1").toInt() != 0;
    m_snapToGrid = root.attribute("snap_grid", "0").toInt() != 0;
    m_snapToObjects = root.attribute("snap_objects", "0").toInt() != 0;
    
    //парсим цвет фона
    QString bgStr = root.attribute("bgcolor", "#0");
    if (bgStr.startsWith("#")) {
        bool ok;
        uint colorVal = bgStr.mid(1).toUInt(&ok, 16);
        if (ok) {
            m_document->setBackgroundColor(QColor(colorVal & 0xFF, (colorVal >> 8) & 0xFF, (colorVal >> 16) & 0xFF));
        }
    }
    
    emit logMessage(tr("Проект: %1 (%2x%3)").arg(m_document->projectName()).arg(m_document->canvasWidth()).arg(m_document->canvasHeight()));
    
    //парсим схемы параметров и сохраняем для использования при сохранении
    QDomElement paramsEl = root.firstChildElement("parameters");
    m_schemas = ObjectsManager::instance()->parseSchemas(paramsEl);
    if (m_schemas.isEmpty()) {
        for (const QString &schemaName : registry->defaultSchemaNames()) {
            m_schemas.insert(schemaName, registry->buildSchema(schemaName));
        }
    }

    QDomElement objectsEl = root.firstChildElement("objects");
    QDomElement fontEl = objectsEl.firstChildElement("font");
    while (!fontEl.isNull()) {
        const FpgaFont font = parseFontElement(fontEl);
        m_fonts.insert(font.index, font);
        fontEl = fontEl.nextSiblingElement("font");
    }
    QDomElement legacyFontsEl = root.firstChildElement("fonts");
    fontEl = legacyFontsEl.firstChildElement("font");
    while (!fontEl.isNull()) {
        const FpgaFont font = parseFontElement(fontEl);
        m_fonts.insert(font.index, font);
        fontEl = fontEl.nextSiblingElement("font");
    }
    
    //маппинг тегов на схемы параметров (для тегов без собственной схемы)
    m_schemaAliases = registry->defaultSchemaAliases();
    //m_schemaAliases["rectangle_e"] = "rectangle";
    
    for (const QString &type : m_schemas.keys()) {
        emit logMessage(tr("Схема: %1 (%2 полей)").arg(type).arg(m_schemas[type].size()));
    }
    
    //парсим объекты
    QDomNode objNode = objectsEl.isNull() ? root.firstChild() : objectsEl.firstChild();
    
    //списки для отладочного дампа
    QList<QDomElement> debugElements;
    QStringList debugTypes;

    while (!objNode.isNull()) {
        QDomElement objEl = objNode.toElement();
        QString tagName = objEl.tagName();
        
        //определяем имя схемы (может отличаться от tagName)
        QString schemaName = m_schemaAliases.value(tagName, tagName);
        if (!m_schemas.contains(schemaName) && m_schemas.contains(tagName)) {
            schemaName = tagName;
        }
        
        if (tagName == QStringLiteral("image")) {
            auto *image = new ImageObject();
            image->x = objEl.attribute("x", "0").toDouble();
            image->y = objEl.attribute("y", "0").toDouble();
            image->width = objEl.attribute("width", "1").toDouble();
            image->height = objEl.attribute("height", "1").toDouble();
            image->format = objEl.attribute("format", "raster");
            image->sourceName = objEl.attribute("source_name");
            image->maskColor = QColor(objEl.attribute("mask_color", "#FFFFFF"));
            image->autoMaskColor = objEl.attribute("mask_color_auto", "1").toInt() != 0;
            image->setViewVisible(objEl.attribute("visible", "1").toInt() != 0);
            image->setExportEnabled(objEl.attribute("export", "1").toInt() != 0);
            image->setSourcePayload(QByteArray::fromBase64(objEl.firstChildElement("source").text().toLatin1()));
            m_objects.append(QSharedPointer<BaseObject>(image));
            m_objectTags.append(QStringLiteral("image"));
        }
        else if (tagName == QStringLiteral("textobject")) {
            auto *textObject = new TextObject();
            textObject->text = objEl.attribute("text");
            textObject->fontFamily = objEl.attribute("font_family", "Arial");
            textObject->pixelSize = objEl.attribute("font_size", "14").toInt();
            textObject->fontIndex = objEl.attribute("font_index", "0").toInt();
            if (!textObject->states.isEmpty()) {
                GroupState &state = textObject->states[0];
                state.x = objEl.attribute("x", "0").toInt();
                state.y = objEl.attribute("y", "0").toInt();
                state.color = QColor(objEl.attribute("color", "#FFFFFF"));
                state.enabled = true;
            }
            textObject->setViewVisible(objEl.attribute("visible", "1").toInt() != 0);
            textObject->setExportEnabled(objEl.attribute("export", "1").toInt() != 0);
            textObject->setObjectProperty(QStringLiteral("Текст"), textObject->text);
            m_objects.append(QSharedPointer<BaseObject>(textObject));
            m_objectTags.append(QStringLiteral("text"));
        }
        else if (tagName == QStringLiteral("text")) {
            const ParamSchema textSchema = registry->buildSchema(QStringLiteral("text"));
            QDomElement initEl = objEl.firstChildElement("init");
            while (!initEl.isNull()) {
                const QString hex = initEl.text().trimmed();
                const int fontIndex = BitParser::extract(hex, textSchema["font_index"].offset, textSchema["font_index"].size);
                auto *textObject = new TextObject();
                textObject->text = initEl.attribute("text");
                textObject->fontIndex = fontIndex;
                if (!textObject->states.isEmpty()) {
                    GroupState &state = textObject->states[0];
                    state.x = BitParser::extract(hex, textSchema["x"].offset, textSchema["x"].size);
                    state.y = BitParser::extract(hex, textSchema["y"].offset, textSchema["y"].size);
                    state.color = BitParser::parseColor(BitParser::extract(hex, textSchema["color"].offset, textSchema["color"].size));
                    state.enabled = BitParser::extract(hex, textSchema["enb"].offset, textSchema["enb"].size) != 0;
                }
                if (m_fonts.contains(fontIndex)) {
                    textObject->setFontAtlas(m_fonts.value(fontIndex), true);
                }
                textObject->setResizeLocked(true);
                textObject->setImportedHardwareObject(true);
                m_objects.append(QSharedPointer<BaseObject>(textObject));
                m_objectTags.append(QStringLiteral("text"));
                initEl = initEl.nextSiblingElement("init");
            }
        }
        else if (tagName == QStringLiteral("font")) {
            // Font resources are parsed before object creation and are not scene objects.
        }
        else if (tagName == QStringLiteral("symbols")) {
            const int fontIndex = objEl.attribute("font_index", "0").toInt();
            const FpgaFont font = m_fonts.value(fontIndex);
            QDomElement symbolEl = objEl.firstChildElement("symbol");
            while (!symbolEl.isNull()) {
                auto *textObject = new TextObject();
                textObject->text = symbolEl.attribute("text");
                textObject->fontIndex = fontIndex;
                textObject->fontFamily = font.name.isEmpty() ? QStringLiteral("Arial") : font.name;
                textObject->pixelSize = font.size > 0 ? font.size : 14;
                const QColor fill(symbolEl.attribute("fill_color", "#FFFFFF"));
                if (!textObject->states.isEmpty()) {
                    QDomElement overallEl = symbolEl.firstChildElement("overal");
                    GroupState &state = textObject->states[0];
                    state.x = overallEl.attribute("left", "0").toInt();
                    state.y = overallEl.attribute("top", "0").toInt();
                    state.color = fill.isValid() ? fill : QColor(Qt::white);
                    state.enabled = true;
                }
                textObject->setFontAtlas(font, true);
                m_objects.append(QSharedPointer<BaseObject>(textObject));
                m_objectTags.append(QStringLiteral("text"));
                symbolEl = symbolEl.nextSiblingElement("symbol");
            }
        }
        else if (m_schemas.contains(schemaName)) {
            QString hexInit = objEl.firstChildElement("init").text().trimmed();
            
            //создаем объект через фабрику
            BaseObject *obj = ObjectsManager::instance()->createObject(tagName);
            
            if (obj && !hexInit.isEmpty()) {
                obj->parse(hexInit, m_schemas[schemaName]);
                obj->parseExtraData(objEl);
                obj->setViewVisible(objEl.attribute("visible", "1").toInt() != 0);
                obj->setExportEnabled(objEl.attribute("export", "1").toInt() != 0);
                
                m_objects.append(QSharedPointer<BaseObject>(obj));
                m_objectTags.append(tagName);
                
                //сохраняем данные для дампа
                debugElements.append(objEl);
                debugTypes.append(tagName);
            }
        }
        
        objNode = objNode.nextSibling();
    }
    
    emit logMessage(tr("Загружено объектов: %1").arg(m_objects.size()));
    if (m_editMode == ProjectEditMode::RestrictedFpgaXml)
        applyRestrictedMode();
    
    //формируем отладочный дамп парсинга
    DebugDumper::dumpToFile(fileName, m_objects, m_schemas, debugElements, debugTypes);
    emit logMessage(tr("Отладочный дамп сформирован"));
    
    emit projectLoaded();
    
    return true;
}

bool ProjectManager::createNewProject(const QString &projectName, int width, int height, const QColor &backgroundColor, const QString &filePath)
{
    auto *registry = FpgaSchemaRegistry::instance();
    m_document->setProjectName(projectName);
    m_document->setCanvasSize(width, height);
    m_document->setBackgroundColor(backgroundColor);
    m_filePath = filePath;
    m_editMode = ProjectEditMode::EditableXml;
    m_showGrid = false;
    m_snapToCanvas = true;
    m_snapToGrid = false;
    m_snapToObjects = false;

    m_objects.clear();
    m_objectTags.clear();
    m_schemaAliases = registry->defaultSchemaAliases();
    resetFontsToDefault();

    m_schemas.clear();
    const QStringList defaultSchemas = registry->defaultSchemaNames();
    for (const QString &schemaName : defaultSchemas) {
        m_schemas.insert(schemaName, registry->buildSchema(schemaName));
    }

    emit logMessage(tr("Создан новый проект: %1 (%2x%3)").arg(m_document->projectName()).arg(m_document->canvasWidth()).arg(m_document->canvasHeight()));
    emit projectLoaded();
    emit projectChanged();
    return true;
}

//метод регистрации стандартых типов объектов (получение словаря поддерживаемых объектов)
void ProjectManager::registerStandardTypes()
{
    //создание экземпляра менеджера объектов
    auto om = ObjectsManager::instance();
    
    //регистрируем типы объектов
    om->registerType("rectangle", []() { return new RectangleObject(); });
    om->registerType("rectangle_a", []() { return new RectangleObject(); });
    //om->registerType("rectangle_e", []() { return new RectangleObject(); });
    om->registerType("dashed_line", []() { return new DashedLineObject(); });
    om->registerType("RibonScale", []() { return new RibbonScaleObject(); });
    om->registerType("ribonscale", []() { return new RibbonScaleObject(); });
    om->registerType("rotationobject", []() { return new RotationObject(); });
    om->registerType("staticgroup", []() { return new StaticGroupObject(); });
    om->registerType("image", []() { return new ImageObject(); });
    om->registerType("aviagorizont", []() { return new AviaHorizonObject(); });
    om->registerType("aviahorizont", []() { return new AviaHorizonObject(); });
    om->registerType("text", []() { return new TextObject(); });
}

int ProjectManager::addObject(const QString &typeName)
{
    auto *registry = FpgaSchemaRegistry::instance();

    if (!m_document->hasCanvas()) {
        emit logMessage(tr("Сначала откройте или создайте проект"));
        return -1;
    }

    BaseObject *rawObject = ObjectsManager::instance()->createObject(typeName);
    if (!rawObject) {
        emit logMessage(tr("Неизвестный тип объекта: %1").arg(typeName));
        return -1;
    }

    const int index = m_objects.size();
    const int offset = 24 * (index % 6);
    const double centerX = qBound(40.0, m_document->canvasWidth() * 0.35 + offset, m_document->canvasWidth() - 40.0);
    const double centerY = qBound(40.0, m_document->canvasHeight() * 0.35 + offset, m_document->canvasHeight() - 40.0);

    if (auto rect = dynamic_cast<RectangleObject*>(rawObject)) {
        rect->x = qMax(12.0, centerX - 60.0);
        rect->y = qMax(12.0, centerY - 40.0);
        rect->width = 120.0;
        rect->height = 80.0;
        rect->fillColor = QColor("#3BA8FF");
        rect->strokeColor = QColor("#EAF6FF");
        rect->strokeWidth = 2.0;
    }
    else if (auto dashedLine = dynamic_cast<DashedLineObject*>(rawObject)) {
        dashedLine->x0 = qMax(12.0, centerX - 70.0);
        dashedLine->y0 = centerY;
        dashedLine->x1 = qMin(static_cast<double>(m_document->canvasWidth() - 12), centerX + 70.0);
        dashedLine->y1 = centerY + 42.0;
        dashedLine->color = QColor("#9EDCFF");
        dashedLine->lineWidth = 3;
        dashedLine->dashPeriod = 16;
        dashedLine->dashLength = 8;
        dashedLine->dashPhase = 0;
    }
    else if (auto ribbon = dynamic_cast<RibbonScaleObject*>(rawObject)) {
        ribbon->left = qMax(12.0, centerX - 48.0);
        ribbon->right = qMin(static_cast<double>(m_document->canvasWidth() - 12), centerX + 48.0);
        ribbon->top = qMax(12.0, centerY - 90.0);
        ribbon->bottom = qMin(static_cast<double>(m_document->canvasHeight() - 12), centerY + 90.0);
        ribbon->lineWidth = 2;
        ribbon->period = 24;
        ribbon->yStart = ribbon->top + 12.0;
        ribbon->color = QColor("#8DE1FF");
    }
    else if (auto horizon = dynamic_cast<AviaHorizonObject*>(rawObject)) {
        horizon->enabled = true;
        horizon->xCenter = centerX;
        horizon->yCenter = centerY;
        horizon->areaWidth = 220.0;
        horizon->areaHeight = 220.0;
        horizon->lineWidth = 4.0;
        horizon->earthColor = QColor("#C27D1B");
        horizon->skyColor = QColor("#4FCAF7");
        horizon->horizonLineColor = QColor("#C0FFFF");
        horizon->sinVal = 0.0;
        horizon->cosVal = 65536.0;
    }
    else if (auto rotation = dynamic_cast<RotationObject*>(rawObject)) {
        const int size = 36;
        rotation->xRot = centerX;
        rotation->yRot = centerY;
        rotation->left = -size / 2.0;
        rotation->top = -size / 2.0;
        rotation->right = size / 2.0;
        rotation->bottom = size / 2.0;
        rotation->sinVal = 0.0;
        rotation->cosVal = 65536.0;
        rotation->color = QColor("#F8FAFC");
        rotation->maskImage = QImage(size, size, QImage::Format_ARGB32);
        rotation->maskImage.fill(QColor(255, 255, 255, 255));
    }
    else if (auto textObject = dynamic_cast<TextObject*>(rawObject)) {
        textObject->pixelSize = 14;
        textObject->fontFamily = QStringLiteral("Arial");
        if (!textObject->states.isEmpty()) {
            textObject->states[0].x = qMax(12, qRound(centerX - textObject->states[0].w / 2.0));
            textObject->states[0].y = qMax(12, qRound(centerY - textObject->states[0].h / 2.0));
        }
    }
    else if (auto staticGroup = dynamic_cast<StaticGroupObject*>(rawObject)) {
        GroupState state;
        state.x = qMax(12, qRound(centerX - 18.0));
        state.y = qMax(12, qRound(centerY - 18.0));
        state.w = 36;
        state.h = 36;
        state.addr = 0;
        state.color = QColor("#89D185");
        state.enabled = true;
        staticGroup->groupNumber = 1;
        staticGroup->states = {state};

        QImage mask(state.w, state.h, QImage::Format_ARGB32);
        mask.fill(QColor(255, 255, 255, 255));
        staticGroup->maskImages = {mask};
    }

    const QString schemaName = registry->canonicalSchemaName(typeName);
    if (!m_schemas.contains(schemaName)) {
        ParamSchema schema = registry->buildSchema(schemaName);
        if (!schema.isEmpty()) {
            m_schemas.insert(schemaName, schema);
        }
    }

    m_objects.append(QSharedPointer<BaseObject>(rawObject));
    m_objectTags.append(registry->canonicalObjectTag(typeName));

    emit projectChanged();
    emit logMessage(tr("Добавлен объект: %1").arg(typeName));
    return m_objects.size() - 1;
}

int ProjectManager::addObject(const QString &typeName, double x, double y)
{
    const int index = addObject(typeName);
    if (index < 0)
        return index;

    auto obj = m_objects[index];
    // Перемещаем объект так, чтобы его центр оказался в точке (x, y)
    const QRectF bounds = obj->getBoundingRect();
    const double dx = x - bounds.center().x();
    const double dy = y - bounds.center().y();
    obj->moveBy(dx, dy);

    emit projectChanged();
    return index;
}

bool ProjectManager::removeObject(int index)
{
    if (index < 0 || index >= m_objects.size())
        return false;

    m_objects.removeAt(index);
    if (index < m_objectTags.size()) {
        m_objectTags.removeAt(index);
    }

    emit projectChanged();
    emit logMessage(tr("Удалён объект #%1").arg(index + 1));
    return true;
}

bool ProjectManager::reorderObjects(const QList<int> &order)
{
    if (order.size() != m_objects.size())
        return false;

    QList<QSharedPointer<BaseObject>> reorderedObjects;
    QStringList reorderedTags;
    reorderedObjects.reserve(m_objects.size());
    reorderedTags.reserve(m_objectTags.size());

    QSet<int> seen;
    for (int index : order) {
        if (index < 0 || index >= m_objects.size() || seen.contains(index))
            return false;

        seen.insert(index);
        reorderedObjects.append(m_objects[index]);
        reorderedTags.append(index < m_objectTags.size() ? m_objectTags[index] : QString());
    }

    m_objects = reorderedObjects;
    m_objectTags = reorderedTags;
    emit projectChanged();
    emit logMessage(tr("Изменён порядок слоёв объектов"));
    return true;
}

bool ProjectManager::alignObject(int index, ObjectAlignment alignment)
{
    if (index < 0 || index >= m_objects.size() || !m_document->hasCanvas())
        return false;

    const auto object = m_objects[index];
    const QRectF bounds = object->getBoundingRect();
    if (bounds.isEmpty())
        return false;

    double dx = 0.0;
    double dy = 0.0;
    switch (alignment) {
    case ObjectAlignment::Left:
        dx = -bounds.left();
        break;
    case ObjectAlignment::HCenter:
        dx = m_document->canvasWidth() / 2.0 - bounds.center().x();
        break;
    case ObjectAlignment::Right:
        dx = m_document->canvasWidth() - bounds.right();
        break;
    case ObjectAlignment::Top:
        dy = -bounds.top();
        break;
    case ObjectAlignment::VCenter:
        dy = m_document->canvasHeight() / 2.0 - bounds.center().y();
        break;
    case ObjectAlignment::Bottom:
        dy = m_document->canvasHeight() - bounds.bottom();
        break;
    }

    object->moveBy(dx, dy);
    emit projectChanged();
    emit logMessage(tr("Выполнено выравнивание объекта"));
    return true;
}

bool ProjectManager::setObjectViewVisible(int index, bool visible)
{
    if (index < 0 || index >= m_objects.size())
        return false;

    m_objects[index]->setViewVisible(visible);
    emit projectChanged();
    return true;
}

bool ProjectManager::setObjectExportEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_objects.size())
        return false;

    m_objects[index]->setExportEnabled(enabled);
    emit projectChanged();
    return true;
}

bool ProjectManager::saveToFile(const QString &targetFile)
{
    const QString outPath = targetFile.isEmpty() ? m_filePath : targetFile;
    if (outPath.isEmpty())
        return false;

    if (m_editMode == ProjectEditMode::RestrictedFpgaXml)
        return exportToFpgaXml(outPath);

    return saveEditableXml(outPath);
}

bool ProjectManager::exportToFpgaXml(const QString &targetFile, const QSet<QString> &alphabetGroups)
{
    const QString outPath = targetFile.isEmpty() ? m_filePath : targetFile;
    if (outPath.isEmpty())
        return false;

    QMap<int, FpgaFont> generatedFonts;
    const QDomDocument doc = buildCompiledXmlDocument(
        m_document->projectName(),
        m_document->canvasWidth(),
        m_document->canvasHeight(),
        m_document->backgroundColor(),
        m_showGrid,
        m_snapToCanvas,
        m_snapToGrid,
        m_snapToObjects,
        m_schemas,
        m_schemaAliases,
        m_objects,
        m_objectTags,
        alphabetGroups,
        &generatedFonts
    );
    
    //записываем XML обратно в файл
    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    
    const QString xmlText = doc.toString(4);
    QStringEncoder encoder("windows-1251");
    QByteArray encodedXml = encoder(xmlText);
    if (encodedXml.isEmpty() && !xmlText.isEmpty()) {
        encodedXml = xmlText.toUtf8();
    }
    outFile.write(encodedXml);
    outFile.close();

    m_fonts = generatedFonts;
    m_filePath = outPath;
    m_editMode = ProjectEditMode::RestrictedFpgaXml;
    applyRestrictedMode();
    emit projectLoaded();
    emit projectChanged();
    emit logMessage(tr("Кадр для ПЛИС сохранён: %1").arg(outPath));
    return true;
}

bool ProjectManager::saveEditableXml(const QString &targetFile)
{
    const QDomDocument doc = buildEditableXmlDocument(
        m_document->projectName(),
        m_document->canvasWidth(),
        m_document->canvasHeight(),
        m_document->backgroundColor(),
        m_showGrid,
        m_snapToCanvas,
        m_snapToGrid,
        m_snapToObjects,
        m_schemas,
        m_schemaAliases,
        m_objects,
        m_objectTags
    );

    QFile outFile(targetFile);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    const QString xmlText = doc.toString(4);
    QStringEncoder encoder("windows-1251");
    QByteArray encodedXml = encoder(xmlText);
    if (encodedXml.isEmpty() && !xmlText.isEmpty())
        encodedXml = xmlText.toUtf8();
    outFile.write(encodedXml);
    outFile.close();

    m_filePath = targetFile;
    m_editMode = ProjectEditMode::EditableXml;
    emit projectLoaded();
    emit logMessage(tr("Проект сохранён: %1").arg(targetFile));
    return true;
}

int ProjectManager::importImageAsStaticGroup(const QString &fileName)
{
    if (!m_document->hasCanvas()) {
        emit logMessage(tr("Сначала создайте или откройте проект"));
        return -1;
    }

    QImage image;
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    auto *imageObject = new ImageObject();
    if (suffix == QStringLiteral("svg")) {
        QSvgRenderer renderer(fileName);
        QFile svgFile(fileName);
        if (renderer.isValid() && svgFile.open(QIODevice::ReadOnly)) {
            const QSize defaultSize = renderer.defaultSize().isValid() ? renderer.defaultSize() : QSize(128, 128);
            imageObject->setSvgData(svgFile.readAll(), QFileInfo(fileName).fileName(), defaultSize);
        } else {
            delete imageObject;
            emit logMessage(tr("Не удалось импортировать SVG: %1").arg(fileName));
            return -1;
        }
    } else {
        image = QImageReader(fileName).read();
        if (image.isNull()) {
            delete imageObject;
            emit logMessage(tr("Не удалось импортировать изображение: %1").arg(fileName));
            return -1;
        }
        imageObject->setRasterImage(image, QFileInfo(fileName).fileName());
    }

    const int maxW = qMax(1, m_document->canvasWidth() / 3);
    const int maxH = qMax(1, m_document->canvasHeight() / 3);
    if (imageObject->width > maxW || imageObject->height > maxH) {
        const QSizeF fitted = QSizeF(imageObject->width, imageObject->height).scaled(QSizeF(maxW, maxH), Qt::KeepAspectRatio);
        imageObject->width = fitted.width();
        imageObject->height = fitted.height();
    }

    imageObject->x = qMax(0.0, (m_document->canvasWidth() - imageObject->width) / 2.0);
    imageObject->y = qMax(0.0, (m_document->canvasHeight() - imageObject->height) / 2.0);

    m_objects.append(QSharedPointer<BaseObject>(imageObject));
    m_objectTags.append(QStringLiteral("image"));
    emit projectChanged();
    emit logMessage(tr("Изображение добавлено: %1").arg(fileName));
    return m_objects.size() - 1;
}

void ProjectManager::applyRestrictedMode()
{
    for (const auto &object : m_objects) {
        object->setImportedHardwareObject(true);
        if (dynamic_cast<StaticGroupObject*>(object.data()) || dynamic_cast<RotationObject*>(object.data())
            || dynamic_cast<TextObject*>(object.data()) || dynamic_cast<ImageObject*>(object.data())) {
            object->setResizeLocked(true);
        }
    }
}

void ProjectManager::resetFontsToDefault()
{
    m_fonts.clear();
    FpgaFont font;
    font.index = 0;
    font.name = QStringLiteral("Arial");
    font.size = 14;
    m_fonts.insert(font.index, font);
}

QSharedPointer<BaseObject> ProjectManager::getObjectAt(int index) const
{
    if (index >= 0 && index < m_objects.size()) {
        return m_objects[index];
    }
    return nullptr;
}

//геттеры
int ProjectManager::getObjectCount() const { return m_objects.size(); }
QString ProjectManager::getProjectName() const { return m_document->projectName(); }
int ProjectManager::getCanvasWidth() const { return m_document->canvasWidth(); }
int ProjectManager::getCanvasHeight() const { return m_document->canvasHeight(); }
QColor ProjectManager::getBackgroundColor() const { return m_document->backgroundColor(); }
QString ProjectManager::getFilePath() const { return m_filePath; }
const QList<QSharedPointer<BaseObject>>& ProjectManager::getObjects() const { return m_objects; }
ProjectEditMode ProjectManager::editMode() const { return m_editMode; }
bool ProjectManager::showGrid() const { return m_showGrid; }
bool ProjectManager::snapToCanvas() const { return m_snapToCanvas; }
bool ProjectManager::snapToGrid() const { return m_snapToGrid; }
bool ProjectManager::snapToObjects() const { return m_snapToObjects; }
QString ProjectManager::editModeName() const
{
    return m_editMode == ProjectEditMode::RestrictedFpgaXml
        ? tr("Ограниченное редактирование XML")
        : tr("Редактируемый XML");
}

int ProjectManager::gridStep() const { return m_gridStep; }
QColor ProjectManager::gridColor() const { return m_gridColor; }

void ProjectManager::reloadGlobalSettings()
{
    QSettings settings("Avionix", "Designer");
    m_gridStep = settings.value("gridStep", 10).toInt();
    
    QString colorStr = settings.value("gridColor", "#3778b4c8").toString();
    m_gridColor = QColor(colorStr);
    
    emit projectChanged();
}

//сеттеры
void ProjectManager::setBackgroundColor(const QColor &color)
{
    if (m_document->backgroundColor() != color) {
        m_document->setBackgroundColor(color);
        emit projectChanged();
    }
}

void ProjectManager::setCanvasSize(int width, int height)
{
    const int clampedWidth = qMax(1, width);
    const int clampedHeight = qMax(1, height);
    if (m_document->canvasWidth() != clampedWidth || m_document->canvasHeight() != clampedHeight) {
        m_document->setCanvasSize(clampedWidth, clampedHeight);
        emit projectChanged();
    }
}

void ProjectManager::setShowGrid(bool enabled)
{
    if (m_showGrid == enabled)
        return;
    m_showGrid = enabled;
    emit projectChanged();
}

void ProjectManager::setSnapToCanvas(bool enabled)
{
    if (m_snapToCanvas == enabled)
        return;
    m_snapToCanvas = enabled;
    emit projectChanged();
}

void ProjectManager::setSnapToGrid(bool enabled)
{
    if (m_snapToGrid == enabled)
        return;
    m_snapToGrid = enabled;
    emit projectChanged();
}

void ProjectManager::setSnapToObjects(bool enabled)
{
    if (m_snapToObjects == enabled)
        return;
    m_snapToObjects = enabled;
    emit projectChanged();
}
