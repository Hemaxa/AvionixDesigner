#include <QFile>
#include <QDomDocument>
#include <QTextStream>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringEncoder>
#include <QImageReader>
#include <QSvgRenderer>
#include <QDataStream>

#include "ProjectManager.h"
#include "EditorProjectDocument.h"
#include "FpgaSchemaRegistry.h"
#include "ObjectsManager.h"
#include "RectangleObject.h"
#include "DashedLineObject.h"
#include "RibbonScaleObject.h"
#include "RotationObject.h"
#include "StaticGroupObject.h"
#include "AviaHorizonObject.h"
#include "TextObject.h"
#include "BitParser.h"
#include "DebugDumper.h"

#include <algorithm>

namespace {
quint32 crc32(const QByteArray &data)
{
    static quint32 table[256] = {};
    static bool initialized = false;
    if (!initialized) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        initialized = true;
    }

    quint32 crc = 0xFFFFFFFFU;
    for (uchar byte : data)
        crc = table[(crc ^ byte) & 0xFFU] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFU;
}

struct ZipEntryData
{
    QString name;
    QByteArray data;
    quint32 crc = 0;
    quint32 offset = 0;
};

bool writeStoredZip(const QString &fileName, QList<ZipEntryData> entries)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    for (ZipEntryData &entry : entries) {
        entry.offset = static_cast<quint32>(file.pos());
        entry.crc = crc32(entry.data);
        const QByteArray name = entry.name.toUtf8();

        out << quint32(0x04034b50);
        out << quint16(20) << quint16(0) << quint16(0) << quint16(0) << quint16(0);
        out << quint32(entry.crc) << quint32(entry.data.size()) << quint32(entry.data.size());
        out << quint16(name.size()) << quint16(0);
        file.write(name);
        file.write(entry.data);
    }

    const quint32 centralOffset = static_cast<quint32>(file.pos());
    for (const ZipEntryData &entry : entries) {
        const QByteArray name = entry.name.toUtf8();
        out << quint32(0x02014b50);
        out << quint16(20) << quint16(20) << quint16(0) << quint16(0) << quint16(0) << quint16(0);
        out << quint32(entry.crc) << quint32(entry.data.size()) << quint32(entry.data.size());
        out << quint16(name.size()) << quint16(0) << quint16(0);
        out << quint16(0) << quint16(0) << quint32(0);
        out << quint32(entry.offset);
        file.write(name);
    }

    const quint32 centralSize = static_cast<quint32>(file.pos()) - centralOffset;
    out << quint32(0x06054b50);
    out << quint16(0) << quint16(0);
    out << quint16(entries.size()) << quint16(entries.size());
    out << quint32(centralSize) << quint32(centralOffset);
    out << quint16(0);
    return true;
}

QByteArray readStoredZipEntry(const QString &fileName, const QString &entryName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    while (!file.atEnd()) {
        quint32 signature = 0;
        in >> signature;
        if (signature != 0x04034b50)
            break;

        quint16 version = 0, flags = 0, method = 0, time = 0, date = 0, nameLen = 0, extraLen = 0;
        quint32 crc = 0, compressedSize = 0, uncompressedSize = 0;
        in >> version >> flags >> method >> time >> date >> crc >> compressedSize >> uncompressedSize >> nameLen >> extraLen;
        const QString name = QString::fromUtf8(file.read(nameLen));
        file.seek(file.pos() + extraLen);
        const QByteArray data = file.read(compressedSize);
        if (method == 0 && name == entryName)
            return data;
    }

    return {};
}

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

QString formatColorRgb(const QColor &color)
{
    return QStringLiteral("#%1%2%3")
        .arg(color.red(), 2, 16, QLatin1Char('0'))
        .arg(color.green(), 2, 16, QLatin1Char('0'))
        .arg(color.blue(), 2, 16, QLatin1Char('0'))
        .toUpper();
}

QString imageToPngBase64(const QImage &image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QString::fromLatin1(bytes.toBase64());
}

QImage imageFromPngBase64(const QString &payload)
{
    QImage image;
    image.loadFromData(QByteArray::fromBase64(payload.toLatin1()), "PNG");
    return image;
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

QImage makeGlyphMask(const QString &text, int width, int height)
{
    QImage image(qMax(1, width), qMax(1, height), QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QFont font(QStringLiteral("Arial"));
    font.setPixelSize(14);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(image.rect(), Qt::AlignLeft | Qt::AlignTop, text);
    painter.end();
    return image;
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

FpgaFont buildFontForExport(const QList<QSharedPointer<BaseObject>> &objects)
{
    FpgaFont font;
    font.index = 0;
    font.name = QStringLiteral("Arial");
    font.size = 14;

    QString characters = QStringLiteral("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    for (const auto &object : objects) {
        if (auto text = dynamic_cast<TextObject*>(object.data())) {
            for (const QChar ch : text->text) {
                if (!ch.isSpace() && !characters.contains(ch))
                    characters.append(ch);
            }
            if (text->hasFontAtlas()) {
                for (auto it = text->fontAtlas().glyphs.constBegin(); it != text->fontAtlas().glyphs.constEnd(); ++it) {
                    if (!font.glyphs.contains(it.key()))
                        font.glyphs.insert(it.key(), it.value());
                }
            }
        }
    }

    int offset = 0;
    for (const QChar ch : characters) {
        if (font.glyphs.contains(ch)) {
            FpgaGlyph glyph = font.glyphs[ch];
            glyph.offset = offset;
            offset += qMax(1, glyph.width * glyph.height);
            font.glyphs[ch] = glyph;
            continue;
        }

        const QString literal(ch);
        const QImage mask = makeGlyphMask(literal, ch.isDigit() ? 10 : 12, 16);
        FpgaGlyph glyph;
        glyph.literal = ch;
        glyph.code = ch.unicode();
        glyph.width = mask.width();
        glyph.height = mask.height();
        glyph.floater = 0;
        glyph.offset = offset;
        glyph.maskRows = maskRowsFromImage(mask);
        glyph.maskImage = mask;
        offset += glyph.width * glyph.height;
        font.glyphs.insert(ch, glyph);
    }

    font.volume = offset * 3;
    return font;
}

QDomElement createGlyphElement(QDomDocument &doc, const FpgaGlyph &glyph)
{
    const QString tagName = glyph.literal.isDigit() ? QStringLiteral("digit") : QStringLiteral("upper");
    QDomElement glyphEl = doc.createElement(tagName);
    glyphEl.setAttribute("literal", QString(glyph.literal));
    glyphEl.setAttribute("code", glyph.code);
    glyphEl.setAttribute("width", glyph.width);
    glyphEl.setAttribute("height", glyph.height);
    glyphEl.setAttribute("floater", glyph.floater);
    glyphEl.setAttribute("offset", glyph.offset);
    glyphEl.appendChild(doc.createTextNode(QStringLiteral("\n%1\n").arg(glyph.maskRows)));
    return glyphEl;
}

FpgaFont parseFontElement(const QDomElement &fontEl)
{
    FpgaFont font;
    font.index = fontEl.attribute("index", "0").toInt();
    font.name = fontEl.attribute("name", "Arial");
    font.size = fontEl.attribute("size", "14").toInt();
    font.volume = fontEl.attribute("volume", "0").toInt();

    QDomElement glyphEl = fontEl.firstChildElement();
    while (!glyphEl.isNull()) {
        const QString tag = glyphEl.tagName();
        if (tag == QStringLiteral("digit") || tag == QStringLiteral("upper")) {
            const QString literal = glyphEl.attribute("literal");
            if (!literal.isEmpty()) {
                FpgaGlyph glyph;
                glyph.literal = literal.front();
                glyph.code = glyphEl.attribute("code", QString::number(glyph.literal.unicode())).toInt();
                glyph.width = glyphEl.attribute("width", "0").toInt();
                glyph.height = glyphEl.attribute("height", "0").toInt();
                glyph.floater = glyphEl.attribute("floater", "0").toInt();
                glyph.offset = glyphEl.attribute("offset", "0").toInt();
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

QJsonObject objectToJson(const QSharedPointer<BaseObject> &object, const QString &tag)
{
    QJsonObject json;
    json["tag"] = tag;
    json["type"] = object->getTypeName();

    if (auto rect = dynamic_cast<RectangleObject*>(object.data())) {
        json["x"] = rect->x;
        json["y"] = rect->y;
        json["width"] = rect->width;
        json["height"] = rect->height;
        json["fillColor"] = rect->fillColor.name();
        json["strokeColor"] = rect->strokeColor.name();
        json["strokeWidth"] = rect->strokeWidth;
        json["alpha"] = rect->alpha;
    } else if (auto line = dynamic_cast<DashedLineObject*>(object.data())) {
        json["enabled"] = line->enabled;
        json["color"] = line->color.name();
        json["x0"] = line->x0;
        json["y0"] = line->y0;
        json["x1"] = line->x1;
        json["y1"] = line->y1;
        json["dashPeriod"] = line->dashPeriod;
        json["dashLength"] = line->dashLength;
        json["dashPhase"] = line->dashPhase;
        json["lineWidth"] = line->lineWidth;
    } else if (auto ribbon = dynamic_cast<RibbonScaleObject*>(object.data())) {
        json["enabled"] = ribbon->enabled;
        json["color"] = ribbon->color.name();
        json["left"] = ribbon->left;
        json["right"] = ribbon->right;
        json["top"] = ribbon->top;
        json["bottom"] = ribbon->bottom;
        json["lineWidth"] = ribbon->lineWidth;
        json["period"] = ribbon->period;
        json["yStart"] = ribbon->yStart;
    } else if (auto horizon = dynamic_cast<AviaHorizonObject*>(object.data())) {
        json["enabled"] = horizon->enabled;
        json["earthColor"] = horizon->earthColor.name();
        json["skyColor"] = horizon->skyColor.name();
        json["horizonLineColor"] = horizon->horizonLineColor.name();
        json["lineWidth"] = horizon->lineWidth;
        json["xCenter"] = horizon->xCenter;
        json["yCenter"] = horizon->yCenter;
        json["areaWidth"] = horizon->areaWidth;
        json["areaHeight"] = horizon->areaHeight;
        json["sinVal"] = horizon->sinVal;
        json["cosVal"] = horizon->cosVal;
    } else if (auto rotation = dynamic_cast<RotationObject*>(object.data())) {
        json["left"] = rotation->left;
        json["top"] = rotation->top;
        json["right"] = rotation->right;
        json["bottom"] = rotation->bottom;
        json["xRot"] = rotation->xRot;
        json["yRot"] = rotation->yRot;
        json["sinVal"] = rotation->sinVal;
        json["cosVal"] = rotation->cosVal;
        json["color"] = rotation->color.name();
        json["maskPng"] = imageToPngBase64(rotation->maskImage);
    } else if (auto text = dynamic_cast<TextObject*>(object.data())) {
        const GroupState state = text->states.isEmpty() ? GroupState{} : text->states.first();
        json["text"] = text->text;
        json["x"] = state.x;
        json["y"] = state.y;
        json["color"] = state.color.name();
        json["fontFamily"] = text->fontFamily;
        json["pixelSize"] = text->pixelSize;
        json["fontIndex"] = text->fontIndex;
        json["kerning"] = text->kerning;
    } else if (auto group = dynamic_cast<StaticGroupObject*>(object.data())) {
        json["groupNumber"] = group->groupNumber;
        QJsonArray states;
        for (int i = 0; i < group->states.size(); ++i) {
            const GroupState &state = group->states[i];
            QJsonObject stateJson;
            stateJson["x"] = state.x;
            stateJson["y"] = state.y;
            stateJson["w"] = state.w;
            stateJson["h"] = state.h;
            stateJson["addr"] = state.addr;
            stateJson["color"] = state.color.name();
            stateJson["enabled"] = state.enabled;
            if (i < group->maskImages.size())
                stateJson["maskPng"] = imageToPngBase64(group->maskImages[i]);
            states.append(stateJson);
        }
        json["states"] = states;
    }

    return json;
}

QDomDocument buildProjectDocument(const QString &projectName,
                                  int canvasWidth,
                                  int canvasHeight,
                                  const QColor &backgroundColor,
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
    doc.appendChild(root);

    QDomElement paramsEl = doc.createElement("parameters");
    root.appendChild(paramsEl);

    auto *registry = FpgaSchemaRegistry::instance();
    QSet<QString> requiredSchemas;
    for (const QString &schemaName : registry->defaultSchemaNames()) {
        requiredSchemas.insert(schemaName);
    }
    for (const QString &tagName : objectTags) {
        requiredSchemas.insert(schemaAliases.value(tagName, registry->canonicalSchemaName(tagName)));
    }

    const QStringList orderedSchemas = registry->orderedSchemaNames();
    for (const QString &schemaName : orderedSchemas) {
        if (!requiredSchemas.contains(schemaName))
            continue;

        QDomElement schemaEl = doc.createElement(schemaName);
        const auto fields = registry->fieldsForSchema(schemaName);
        for (const auto &field : fields) {
            QDomElement fieldEl = doc.createElement(field.name);
            fieldEl.setAttribute("offset", field.offset);
            fieldEl.setAttribute("size", field.size);
            schemaEl.appendChild(fieldEl);
        }
        paramsEl.appendChild(schemaEl);
    }

    for (const QString &schemaName : requiredSchemas) {
        if (orderedSchemas.contains(schemaName))
            continue;

        if (!schemas.contains(schemaName))
            continue;

        QDomElement schemaEl = doc.createElement(schemaName);
        const auto fields = registry->fieldsForSchema(schemaName);
        for (const auto &field : fields) {
            QDomElement fieldEl = doc.createElement(field.name);
            fieldEl.setAttribute("offset", field.offset);
            fieldEl.setAttribute("size", field.size);
            schemaEl.appendChild(fieldEl);
        }
        paramsEl.appendChild(schemaEl);
    }

    QDomElement objectsEl = doc.createElement("objects");
    root.appendChild(objectsEl);

    for (int objIdx = 0; objIdx < objects.size(); ++objIdx) {
        if (dynamic_cast<TextObject*>(objects[objIdx].data()))
            continue;

        const QString tagName = objIdx < objectTags.size() ? objectTags[objIdx] : QString();
        const QString schemaName = schemaAliases.value(tagName, registry->canonicalSchemaName(tagName));
        if (tagName.isEmpty() || !schemas.contains(schemaName))
            continue;

        objectsEl.appendChild(createObjectElement(doc, tagName, schemas[schemaName], objects[objIdx]));
    }

    QList<TextObject*> textObjects;
    for (const auto &object : objects) {
        if (auto text = dynamic_cast<TextObject*>(object.data()))
            textObjects.append(text);
    }

    if (!textObjects.isEmpty()) {
        FpgaFont exportFont = buildFontForExport(objects);
        QDomElement symbolsEl = doc.createElement("symbols");
        symbolsEl.setAttribute("font_index", exportFont.index);
        symbolsEl.setAttribute("count", textObjects.size());

        for (int i = 0; i < textObjects.size(); ++i) {
            TextObject *text = textObjects[i];
            const QRect overall = text->overallRect();
            QDomElement symbolEl = doc.createElement("symbol");
            symbolEl.setAttribute("index", i);
            symbolEl.setAttribute("text", text->text);
            const GroupState state = text->states.isEmpty() ? GroupState{} : text->states.first();
            symbolEl.setAttribute("fill_color", formatColorRgb(state.color));

            QDomElement overallEl = doc.createElement("overal");
            overallEl.setAttribute("left", overall.left());
            overallEl.setAttribute("right", overall.left() + overall.width());
            overallEl.setAttribute("top", overall.top());
            overallEl.setAttribute("bottom", overall.top() + overall.height());
            symbolEl.appendChild(overallEl);
            symbolsEl.appendChild(symbolEl);
        }

        objectsEl.appendChild(symbolsEl);

        QDomElement fontsEl = doc.createElement("fonts");
        fontsEl.setAttribute("count", 1);
        QDomElement fontEl = doc.createElement("font");
        fontEl.setAttribute("index", exportFont.index);
        fontEl.setAttribute("name", exportFont.name);
        fontEl.setAttribute("size", exportFont.size);
        fontEl.setAttribute("volume", exportFont.volume);

        QList<QChar> glyphKeys = exportFont.glyphs.keys();
        std::sort(glyphKeys.begin(), glyphKeys.end(), [](const QChar &a, const QChar &b) {
            return a.unicode() < b.unicode();
        });
        for (const QChar key : glyphKeys) {
            fontEl.appendChild(createGlyphElement(doc, exportFont.glyphs[key]));
        }
        fontsEl.appendChild(fontEl);
        root.appendChild(fontsEl);
    }

    return doc;
}
}

ProjectManager::ProjectManager() : m_document(new EditorProjectDocument()) {}

ProjectManager* ProjectManager::instance()
{
    static ProjectManager s_instance;
    return &s_instance;
}

bool ProjectManager::loadFromFile(const QString &fileName)
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix == QStringLiteral("avd"))
        return loadAvdProject(fileName);
    return loadXmlProject(fileName);
}

bool ProjectManager::loadXmlProject(const QString &fileName)
{
    auto *registry = FpgaSchemaRegistry::instance();
    m_filePath = fileName;
    m_editMode = ProjectEditMode::RestrictedFpgaXml;
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
    
    //читаем метаданные проекта
    m_document->setProjectName(root.attribute("name", "Untitled"));
    m_document->setCanvasSize(root.attribute("width", "640").toInt(), root.attribute("height", "480").toInt());
    m_document->setBackgroundColor(Qt::black);
    
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

    QDomElement fontsEl = root.firstChildElement("fonts");
    QDomElement fontEl = fontsEl.firstChildElement("font");
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
    QDomElement objectsEl = root.firstChildElement("objects");
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
        
        if (tagName == QStringLiteral("symbols")) {
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
    m_editMode = ProjectEditMode::EditableProject;

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

bool ProjectManager::saveToFile(const QString &targetFile)
{
    const QString outPath = targetFile.isEmpty() ? m_filePath : targetFile;
    if (outPath.isEmpty())
        return false;

    if (QFileInfo(outPath).suffix().toLower() == QStringLiteral("avd"))
        return saveAvdProject(outPath);

    return exportToFpgaXml(outPath);
}

bool ProjectManager::exportToFpgaXml(const QString &targetFile)
{
    const QString outPath = targetFile.isEmpty() ? m_filePath : targetFile;
    if (outPath.isEmpty())
        return false;

    const QDomDocument doc = buildProjectDocument(
        m_document->projectName(),
        m_document->canvasWidth(),
        m_document->canvasHeight(),
        m_document->backgroundColor(),
        m_schemas,
        m_schemaAliases,
        m_objects,
        m_objectTags
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
    
    emit logMessage(tr("Кадр для ПЛИС сохранён: %1").arg(outPath));
    return true;
}

bool ProjectManager::saveAvdProject(const QString &targetFile)
{
    QJsonObject root;
    root["format"] = QStringLiteral("AvionixDesignerProject");
    root["version"] = 1;
    root["name"] = m_document->projectName();
    root["width"] = m_document->canvasWidth();
    root["height"] = m_document->canvasHeight();
    root["backgroundColor"] = m_document->backgroundColor().name();

    QJsonArray objects;
    for (int i = 0; i < m_objects.size(); ++i) {
        const QString tag = i < m_objectTags.size() ? m_objectTags[i] : QString();
        objects.append(objectToJson(m_objects[i], tag));
    }
    root["objects"] = objects;

    QList<ZipEntryData> entries;
    entries.append({QStringLiteral("project.json"), QJsonDocument(root).toJson(QJsonDocument::Indented)});
    entries.append({QStringLiteral("assets/.keep"), QByteArray()});
    if (!writeStoredZip(targetFile, entries))
        return false;

    m_filePath = targetFile;
    m_editMode = ProjectEditMode::EditableProject;
    emit projectLoaded();
    emit logMessage(tr("Проект сохранён: %1").arg(targetFile));
    return true;
}

bool ProjectManager::loadAvdProject(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray fileBytes = file.readAll();
    file.close();

    QByteArray projectJson = fileBytes;
    if (fileBytes.startsWith("PK")) {
        projectJson = readStoredZipEntry(fileName, QStringLiteral("project.json"));
    }

    const QJsonDocument jsonDoc = QJsonDocument::fromJson(projectJson);
    if (!jsonDoc.isObject())
        return false;

    auto *registry = FpgaSchemaRegistry::instance();
    const QJsonObject root = jsonDoc.object();

    m_document->setProjectName(root.value("name").toString("Untitled"));
    m_document->setCanvasSize(root.value("width").toInt(640), root.value("height").toInt(480));
    const QColor bg(root.value("backgroundColor").toString("#000000"));
    m_document->setBackgroundColor(bg.isValid() ? bg : Qt::black);
    m_filePath = fileName;
    m_editMode = ProjectEditMode::EditableProject;
    m_objects.clear();
    m_objectTags.clear();
    m_schemaAliases = registry->defaultSchemaAliases();
    resetFontsToDefault();

    m_schemas.clear();
    for (const QString &schemaName : registry->defaultSchemaNames()) {
        m_schemas.insert(schemaName, registry->buildSchema(schemaName));
    }

    const QJsonArray objects = root.value("objects").toArray();
    for (const QJsonValue &value : objects) {
        const QJsonObject objJson = value.toObject();
        const QString tag = objJson.value("tag").toString();
        const QString createType = tag.isEmpty() ? objJson.value("type").toString() : tag;
        BaseObject *raw = ObjectsManager::instance()->createObject(createType);
        if (!raw)
            continue;

        if (auto rect = dynamic_cast<RectangleObject*>(raw)) {
            rect->x = objJson.value("x").toDouble();
            rect->y = objJson.value("y").toDouble();
            rect->width = objJson.value("width").toDouble(1.0);
            rect->height = objJson.value("height").toDouble(1.0);
            rect->fillColor = QColor(objJson.value("fillColor").toString("#3BA8FF"));
            rect->strokeColor = QColor(objJson.value("strokeColor").toString("#EAF6FF"));
            rect->strokeWidth = objJson.value("strokeWidth").toDouble(1.0);
            rect->alpha = objJson.value("alpha").toInt(255);
        } else if (auto line = dynamic_cast<DashedLineObject*>(raw)) {
            line->enabled = objJson.value("enabled").toBool(true);
            line->color = QColor(objJson.value("color").toString("#F8FAFC"));
            line->x0 = objJson.value("x0").toDouble();
            line->y0 = objJson.value("y0").toDouble();
            line->x1 = objJson.value("x1").toDouble(80.0);
            line->y1 = objJson.value("y1").toDouble();
            line->dashPeriod = objJson.value("dashPeriod").toInt(16);
            line->dashLength = objJson.value("dashLength").toInt(8);
            line->dashPhase = objJson.value("dashPhase").toInt(0);
            line->lineWidth = objJson.value("lineWidth").toInt(2);
        } else if (auto ribbon = dynamic_cast<RibbonScaleObject*>(raw)) {
            ribbon->enabled = objJson.value("enabled").toBool(true);
            ribbon->color = QColor(objJson.value("color").toString("#8DE1FF"));
            ribbon->left = objJson.value("left").toDouble();
            ribbon->right = objJson.value("right").toDouble(160.0);
            ribbon->top = objJson.value("top").toDouble();
            ribbon->bottom = objJson.value("bottom").toDouble(220.0);
            ribbon->lineWidth = objJson.value("lineWidth").toInt(2);
            ribbon->period = objJson.value("period").toInt(24);
            ribbon->yStart = objJson.value("yStart").toDouble();
        } else if (auto horizon = dynamic_cast<AviaHorizonObject*>(raw)) {
            horizon->enabled = objJson.value("enabled").toBool(true);
            horizon->earthColor = QColor(objJson.value("earthColor").toString("#C27D1B"));
            horizon->skyColor = QColor(objJson.value("skyColor").toString("#4FCAF7"));
            horizon->horizonLineColor = QColor(objJson.value("horizonLineColor").toString("#C0FFFF"));
            horizon->lineWidth = objJson.value("lineWidth").toDouble(4.0);
            horizon->xCenter = objJson.value("xCenter").toDouble();
            horizon->yCenter = objJson.value("yCenter").toDouble();
            horizon->areaWidth = objJson.value("areaWidth").toDouble(220.0);
            horizon->areaHeight = objJson.value("areaHeight").toDouble(220.0);
            horizon->sinVal = objJson.value("sinVal").toDouble();
            horizon->cosVal = objJson.value("cosVal").toDouble(65536.0);
        } else if (auto rotation = dynamic_cast<RotationObject*>(raw)) {
            rotation->left = objJson.value("left").toDouble();
            rotation->top = objJson.value("top").toDouble();
            rotation->right = objJson.value("right").toDouble(1.0);
            rotation->bottom = objJson.value("bottom").toDouble(1.0);
            rotation->xRot = objJson.value("xRot").toDouble();
            rotation->yRot = objJson.value("yRot").toDouble();
            rotation->sinVal = objJson.value("sinVal").toDouble();
            rotation->cosVal = objJson.value("cosVal").toDouble(65536.0);
            rotation->color = QColor(objJson.value("color").toString("#FFFFFF"));
            rotation->maskImage = imageFromPngBase64(objJson.value("maskPng").toString());
        } else if (auto text = dynamic_cast<TextObject*>(raw)) {
            text->text = objJson.value("text").toString("TEXT");
            text->fontFamily = QStringLiteral("Arial");
            text->pixelSize = 14;
            text->fontIndex = objJson.value("fontIndex").toInt(0);
            text->kerning = objJson.value("kerning").toInt(1);
            text->setObjectProperty("Текст", text->text);
            if (!text->states.isEmpty()) {
                GroupState &state = text->states[0];
                state.x = objJson.value("x").toInt(60);
                state.y = objJson.value("y").toInt(60);
                const QColor color(objJson.value("color").toString("#F8FAFC"));
                state.color = color.isValid() ? color : QColor(Qt::white);
            }
            text->setObjectProperty("Текст", text->text);
        } else if (auto group = dynamic_cast<StaticGroupObject*>(raw)) {
            group->groupNumber = objJson.value("groupNumber").toInt(1);
            group->states.clear();
            group->maskImages.clear();
            const QJsonArray states = objJson.value("states").toArray();
            for (const QJsonValue &stateValue : states) {
                const QJsonObject stateJson = stateValue.toObject();
                GroupState state;
                state.x = stateJson.value("x").toInt();
                state.y = stateJson.value("y").toInt();
                state.w = stateJson.value("w").toInt(1);
                state.h = stateJson.value("h").toInt(1);
                state.addr = stateJson.value("addr").toInt();
                state.color = QColor(stateJson.value("color").toString("#FFFFFF"));
                state.enabled = stateJson.value("enabled").toBool(true);
                group->states.append(state);
                group->maskImages.append(imageFromPngBase64(stateJson.value("maskPng").toString()));
            }
        }

        m_objects.append(QSharedPointer<BaseObject>(raw));
        m_objectTags.append(tag.isEmpty() ? registry->canonicalObjectTag(createType) : tag);
    }

    emit projectLoaded();
    emit projectChanged();
    emit logMessage(tr("Проект загружен: %1").arg(fileName));
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
    if (suffix == QStringLiteral("svg")) {
        QSvgRenderer renderer(fileName);
        if (renderer.isValid()) {
            const QSize defaultSize = renderer.defaultSize().isValid() ? renderer.defaultSize() : QSize(128, 128);
            image = QImage(defaultSize, QImage::Format_ARGB32);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            renderer.render(&painter);
        }
    } else {
        image = QImageReader(fileName).read();
    }

    if (image.isNull()) {
        emit logMessage(tr("Не удалось импортировать изображение: %1").arg(fileName));
        return -1;
    }

    auto *group = new StaticGroupObject();
    const int maxW = qMax(1, m_document->canvasWidth() / 3);
    const int maxH = qMax(1, m_document->canvasHeight() / 3);
    if (image.width() > maxW || image.height() > maxH) {
        image = image.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    GroupState state;
    state.w = image.width();
    state.h = image.height();
    state.x = qMax(0, (m_document->canvasWidth() - state.w) / 2);
    state.y = qMax(0, (m_document->canvasHeight() - state.h) / 2);
    state.color = QColor("#FFFFFF");
    state.enabled = true;
    group->groupNumber = 1;
    group->states = {state};
    group->maskImages = {image.convertToFormat(QImage::Format_ARGB32)};

    m_objects.append(QSharedPointer<BaseObject>(group));
    m_objectTags.append(QStringLiteral("staticgroup"));
    emit projectChanged();
    emit logMessage(tr("Изображение добавлено: %1").arg(fileName));
    return m_objects.size() - 1;
}

void ProjectManager::applyRestrictedMode()
{
    for (const auto &object : m_objects) {
        object->setImportedHardwareObject(true);
        if (dynamic_cast<StaticGroupObject*>(object.data()) || dynamic_cast<RotationObject*>(object.data()) || dynamic_cast<TextObject*>(object.data())) {
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
QString ProjectManager::editModeName() const
{
    return m_editMode == ProjectEditMode::RestrictedFpgaXml
        ? tr("Ограниченное редактирование XML")
        : tr("Проект .avd");
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
