#include "FpgaXmlExporter.h"
#include "FpgaScalePlaceholder.h"

#include "ImageResourceBuilder.h"
#include "FpgaSchemaRegistry.h"
#include "BitParser.h"
#include "ImageObject.h"
#include "RectangleObject.h"
#include "RotationObject.h"
#include "StaticGroupObject.h"
#include "TextObject.h"
#include "ThreeDViewObject.h"
#include "text/FontRasterizer.h"
#include <QFontMetrics>
#include <QSet>
#include <QtMath>

namespace {
// Поля заполняются по смещениям схемы; порядок XML задаётся отдельно реестром.
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

// Маска содержит только квантованный alpha; аппаратный цвет хранится в init.
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
        const int startAddr = qMax(0, state.addr);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int idx = startAddr + y * qMax(1, state.w) + x;
                if (idx >= values.size())
                    break;

                const int alpha = image.pixelColor(x, y).alpha();
                values[idx] = qBound(0, qRound(alpha * 7.0 / 255.0), 7);
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

// Параметрическим объектам достаточно init, растровым добавляется общая data.
QDomElement createObjectElement(QDomDocument &doc, const QString &tagName, const ParamSchema &schema, const QSharedPointer<BaseObject> &obj)
{
    QDomElement objEl = doc.createElement(tagName);

    if (auto threeDView = dynamic_cast<ThreeDViewObject*>(obj.data())) {
        QDomElement initEl = doc.createElement("init");
        initEl.appendChild(doc.createTextNode(threeDView->serializeInit()));
        objEl.appendChild(initEl);
        return objEl;
    }

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

    if (auto rotation = dynamic_cast<RotationObject*>(obj.data())) {
        const QList<QMap<QString, quint32>> layerParams = rotation->serializeLayerParams();
        for (int layerIndex = 0; layerIndex < layerParams.size(); ++layerIndex) {
            QDomElement initEl = doc.createElement("init");
            initEl.setAttribute("index", layerIndex + 1);
            initEl.appendChild(doc.createTextNode(buildInitHex(schema, layerParams[layerIndex])));
            objEl.appendChild(initEl);
        }

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
        return objEl;
    }

    QDomElement initEl = doc.createElement("init");
    initEl.appendChild(doc.createTextNode(buildInitHex(schema, obj->serializeParams())));
    objEl.appendChild(initEl);

    return objEl;
}

// Алфавит ресурса собирается заранее, чтобы UART мог менять текст без загрузки новых масок.
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
    if (groups.contains(QStringLiteral("symbols")))
        appendUnique(QStringLiteral("α°+-/"));

    return characters;
}

QSet<QString> detectAlphabetCategories(const QString &characters)
{
    QSet<QString> groups;
    for (const QChar ch : characters) {
        const ushort u = ch.unicode();
        if (ch.isDigit()) {
            groups.insert(QStringLiteral("digits"));
        } else if (u >= 'A' && u <= 'Z') {
            groups.insert(QStringLiteral("latin_upper"));
        } else if (u >= 'a' && u <= 'z') {
            groups.insert(QStringLiteral("latin_lower"));
        } else if ((u >= 0x0410 && u <= 0x042F) || u == 0x0401) {
            groups.insert(QStringLiteral("cyrillic_upper"));
        } else if ((u >= 0x0430 && u <= 0x044F) || u == 0x0451) {
            groups.insert(QStringLiteral("cyrillic_lower"));
        } else if (QStringLiteral("α°+-/").contains(ch)) {
            groups.insert(QStringLiteral("symbols"));
        }
    }
    return groups;
}

QString completeAlphabetForTextObjects(const QList<TextObject*> &texts)
{
    QSet<QString> groups;
    QString explicitCharacters;

    auto appendUniqueCharacter = [&explicitCharacters](const QChar ch) {
        if (!explicitCharacters.contains(ch))
            explicitCharacters.append(ch);
    };

    for (const TextObject *text : texts) {
        if (!text)
            continue;

        const QString characters = text->exportCharacters();
        if (text->exportAlphabetGroups.isEmpty()) {
            groups.unite(detectAlphabetCategories(characters));
        } else {
            for (const QString &group : text->exportAlphabetGroups)
                groups.insert(group);
        }
        for (const QChar ch : characters) {
            if (ch.isSpace()) {
                appendUniqueCharacter(ch);
                continue;
            }
            if (detectAlphabetCategories(QString(ch)).isEmpty())
                appendUniqueCharacter(ch);
        }
    }

    QString characters = alphabetCharacters(groups);
    for (const QChar ch : explicitCharacters) {
        if (!characters.contains(ch))
            characters.append(ch);
    }
    return characters;
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

QStringList normalizedGlyphValues(const FpgaGlyph &glyph)
{
    QStringList values = maskRowsToValues(glyph.maskRows);
    const int expectedSize = qMax(1, glyph.width) * qMax(1, glyph.height);
    while (values.size() < expectedSize)
        values.append(QStringLiteral("0"));
    while (values.size() > expectedSize)
        values.removeLast();
    return values;
}

void insertGlyph(FpgaFont *font, const FpgaGlyph &glyph)
{
    if (!font)
        return;

    font->glyphs.insert(glyph.literal, glyph);
    if (!font->glyphOrder.contains(glyph.literal))
        font->glyphOrder.append(glyph.literal);
}

FpgaGlyph normalizedGlyph(FpgaGlyph glyph, const QChar literal, int fontSize)
{
    glyph.literal = literal;
    if (glyph.code == 0)
        glyph.code = literal.unicode();

    const QStringList rows = glyph.maskRows.split('\n', Qt::SkipEmptyParts);
    if (glyph.height <= 0)
        glyph.height = qMax(1, rows.size());
    if (glyph.width <= 0) {
        int maxWidth = 0;
        for (const QString &row : rows)
            maxWidth = qMax(maxWidth, row.trimmed().size());
        glyph.width = qMax(1, maxWidth);
    }

    if (glyph.advance < 0)
        glyph.advance = glyph.width;
    if (glyph.ascent <= 0 && glyph.descent <= 0)
        glyph.ascent = glyph.height;
    if (glyph.size <= 0)
        glyph.size = fontSize;

    if (glyph.maskRows.trimmed().isEmpty()) {
        QStringList zeroRows;
        for (int y = 0; y < glyph.height; ++y)
            zeroRows.append(QString(glyph.width, QLatin1Char('0')));
        glyph.maskRows = zeroRows.join('\n');
    }

    return glyph;
}

FpgaGlyph transparentSpaceGlyph(int fontSize)
{
    const int advance = qMax(4, fontSize / 2);
    FpgaGlyph glyph;
    glyph.literal = QLatin1Char(' ');
    glyph.code = QLatin1Char(' ').unicode();
    glyph.width = advance;
    glyph.height = 1;
    glyph.advance = advance;
    glyph.size = fontSize;
    glyph.maskRows = QString(advance, QLatin1Char('0'));
    return glyph;
}

QString objectTagForSerialization(int objIdx,
                                  const QList<QSharedPointer<BaseObject>> &objects,
                                  const QStringList &objectTags)
{
    if (objIdx >= 0 && objIdx < objects.size()) {
        if (auto rectangle = dynamic_cast<RectangleObject*>(objects[objIdx].data()))
            return rectangle->usesAlpha() ? QStringLiteral("rectangle_a") : QStringLiteral("rectangle");
    }

    return objIdx >= 0 && objIdx < objectTags.size() ? objectTags[objIdx] : QString();
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

QSet<QString> collectCompiledSchemaNames(const QList<QSharedPointer<BaseObject>> &objects,
                                         const QStringList &objectTags)
{
    QSet<QString> schemaNames;

    for (int objIdx = objects.size() - 1; objIdx >= 0; --objIdx) {
        const auto &obj = objects[objIdx];
        if (!obj->isExportEnabled())
            continue;

        if (auto image = dynamic_cast<ImageObject*>(obj.data())) {
            schemaNames.insert(image->isRotationObject() ? QStringLiteral("rotationobject") : QStringLiteral("staticgroup"));
            continue;
        }

        if (dynamic_cast<TextObject*>(obj.data())) {
            schemaNames.insert(QStringLiteral("font"));
            schemaNames.insert(QStringLiteral("text"));
            continue;
        }

        const QString tagName = objectTagForSerialization(objIdx, objects, objectTags);
        if (tagName.isEmpty())
            continue;

        schemaNames.insert(tagName);
    }

    return schemaNames;
}

QDomElement createStaticGroupElementFromImageLayers(QDomDocument &doc,
                                                   const ParamSchema &schema,
                                                   const ImageObject *image,
                                                   const QList<ImageMaskComponent> &components)
{
    StaticGroupObject group;
    int nextAddr = 0;

    if (components.isEmpty()) {
        GroupState state;
        state.x = qRound(image->x);
        state.y = qRound(image->y);
        state.w = qMax(1, qRound(image->width));
        state.h = qMax(1, qRound(image->height));
        state.addr = 0;
        state.color = image->effectiveMaskColor();
        state.enabled = true;
        group.states = {state};
        group.maskImages = {image->renderedImage()};
    } else {
        for (const ImageMaskComponent &component : components) {
            GroupState state;
            state.x = qRound(image->x + component.bounds.left());
            state.y = qRound(image->y + component.bounds.top());
            state.w = qMax(1, component.bounds.width());
            state.h = qMax(1, component.bounds.height());
            state.addr = nextAddr;
            state.color = component.color;
            state.enabled = true;
            group.states.append(state);
            group.maskImages.append(component.mask);
            nextAddr += state.w * state.h;
        }
    }

    group.groupNumber = qMax(1, group.states.size());
    return createObjectElement(doc, QStringLiteral("staticgroup"), schema, QSharedPointer<BaseObject>(&group, [](BaseObject*){}));
}

void appendStaticGroupElementsFromImageLayers(QDomDocument &doc,
                                              QDomElement &objectsEl,
                                              const ParamSchema &schema,
                                              const ImageObject *image)
{
    QList<ImageMaskComponent> components = image->maskComponents();
    if (components.isEmpty()) {
        ImageMaskComponent component;
        component.bounds = QRect(0, 0, qMax(1, qRound(image->width)), qMax(1, qRound(image->height)));
        component.color = image->effectiveMaskColor();
        component.mask = image->renderedImage();
        components.append(component);
    }

    const QList<ImageMaskComponent> simplifiedComponents = ImageResourceBuilder::simplifyComponentsForStaticGroup(schema, components);
    if (simplifiedComponents.isEmpty()) {
        objectsEl.appendChild(createStaticGroupElementFromImageLayers(doc, schema, image, {}));
        return;
    }

    objectsEl.appendChild(createStaticGroupElementFromImageLayers(doc, schema, image, simplifiedComponents));
}

QDomElement createRotationObjectElementFromImageComponent(QDomDocument &doc,
                                                         const ParamSchema &schema,
                                                         const ImageObject *image,
                                                         const ImageMaskComponent &component)
{
    RotationObject rotation;
    rotation.xRot = qRound(image->x + image->width / 2.0);
    rotation.yRot = qRound(image->y + image->height / 2.0);

    const double componentLeft = image->x + component.bounds.left();
    const double componentTop = image->y + component.bounds.top();
    rotation.left = qRound(componentLeft - rotation.xRot);
    rotation.top = qRound(componentTop - rotation.yRot);
    rotation.right = rotation.left + qMax(1, component.bounds.width());
    rotation.bottom = rotation.top + qMax(1, component.bounds.height());
    rotation.color = component.color;
    rotation.maskImage = component.mask;
    rotation.sinVal = image->rotationCoefficients().x();
    rotation.cosVal = image->rotationCoefficients().y();

    return createObjectElement(doc, QStringLiteral("rotationobject"), schema, QSharedPointer<BaseObject>(&rotation, [](BaseObject*){}));
}

void appendCompiledImageElements(QDomDocument &doc,
                                 QDomElement &objectsEl,
                                 const ParamSchema &staticSchema,
                                 const ParamSchema &rotationSchema,
    const ImageObject *image)
{
    if (!image->isRotationObject()) {
        appendStaticGroupElementsFromImageLayers(doc, objectsEl, staticSchema, image);
        return;
    }

    ImageMaskComponent component = ImageResourceBuilder::singleComponentForRotationObject(image);
    if (component.mask.isNull() || component.bounds.isEmpty()) {
        component.bounds = QRect(0, 0, qMax(1, qRound(image->width)), qMax(1, qRound(image->height)));
        component.color = image->effectiveMaskColor();
        component.mask = image->renderedImage();
    }

    objectsEl.appendChild(createRotationObjectElementFromImageComponent(doc,
                                                                        rotationSchema,
                                                                        image,
                                                                        ImageResourceBuilder::simplifyComponentForRotationObject(component)));
}

// ExportFontKey - ключ объединения текстовых объектов в один ресурс шрифта.
struct ExportFontKey
{
    QString family;
    int size = 14;
    bool existingAtlas = false;
    int sourceFontIndex = -1;

    bool operator<(const ExportFontKey &other) const
    {
        if (existingAtlas != other.existingAtlas)
            return existingAtlas < other.existingAtlas;
        if (sourceFontIndex != other.sourceFontIndex)
            return sourceFontIndex < other.sourceFontIndex;
        if (family != other.family)
            return family < other.family;
        return size < other.size;
    }
};

ExportFontKey exportFontKeyForText(const TextObject *text)
{
    if (!text)
        return {};

    if (text->hasFontAtlas()) {
        return {
            text->fontFamily,
            text->pixelSize,
            true,
            text->fontAtlas().index
        };
    }

    return {text->fontFamily, text->pixelSize, false, -1};
}

QDomElement createFontResourceElement(QDomDocument &doc,
                                      const ExportFontKey &key,
                                      int fontIndex,
                                      const QString &characters,
                                      const ParamSchema &fontSchema)
{
    FpgaFont font;
    font.index = fontIndex;
    font.name = key.family;
    font.size = key.size;

    QDomElement fontEl = doc.createElement("font");
    fontEl.setAttribute("index", fontIndex);
    fontEl.setAttribute("name", key.family);
    fontEl.setAttribute("size", key.size);

    QFont qfont(key.family);
    qfont.setPixelSize(key.size);

    QStringList dataValues;
    int offset = 0;
    int glyphIndex = 0;
    for (const QChar ch : characters) {
        FpgaGlyph glyph = rasterizeGlyph(qfont, ch);
        glyph.offset = offset;
        glyph.size = key.size;
        glyphIndex++;

        const QStringList glyphValues = normalizedGlyphValues(glyph);
        offset += glyphValues.size();
        dataValues.append(glyphValues);
        insertGlyph(&font, glyph);

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
    fontEl.setAttribute("count", glyphIndex);

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
        }
    }
    // Один порядок пар для системного шрифта и восстановленного XML-атласа.
    for (auto it = font.kerningPairs.cbegin(); it != font.kerningPairs.cend(); ++it) {
        auto kerningEl = doc.createElement("kerning");
        kerningEl.setAttribute("left", QString(it.key()[0]));
        kerningEl.setAttribute("right", QString(it.key()[1]));
        kerningEl.setAttribute("delta", it.value());
        fontEl.appendChild(kerningEl);
    }

    QDomElement dataEl = doc.createElement("data");
    dataEl.appendChild(doc.createTextNode(dataValues.join(", ")));
    fontEl.appendChild(dataEl);

    return fontEl;
}

QDomElement createFontResourceElementFromAtlas(QDomDocument &doc,
                                               const FpgaFont &sourceFont,
                                               int fontIndex,
                                               const QString &characters,
                                               const ParamSchema &fontSchema)
{
    FpgaFont font;
    font.index = fontIndex;
    font.name = sourceFont.name;
    font.size = sourceFont.size;

    QDomElement fontEl = doc.createElement("font");
    fontEl.setAttribute("index", fontIndex);
    fontEl.setAttribute("name", sourceFont.name);
    fontEl.setAttribute("size", sourceFont.size);

    QStringList dataValues;
    int offset = 0;
    int glyphIndex = 0;
    for (const QChar ch : characters) {
        if (!sourceFont.glyphs.contains(ch) && !ch.isSpace())
            continue;

        FpgaGlyph glyph = ch.isSpace() && !sourceFont.glyphs.contains(ch)
            ? transparentSpaceGlyph(sourceFont.size)
            : sourceFont.glyphs.value(ch);
        glyph = normalizedGlyph(glyph, ch, sourceFont.size);
        glyph.offset = offset;
        glyph.size = sourceFont.size;
        glyphIndex++;

        const QStringList glyphValues = normalizedGlyphValues(glyph);
        offset += glyphValues.size();
        dataValues.append(glyphValues);
        insertGlyph(&font, glyph);

        QDomElement initEl = doc.createElement("init");
        initEl.setAttribute("index", glyphIndex);
        initEl.setAttribute("literal", QString(ch));
        initEl.setAttribute("code", glyph.code);
        initEl.appendChild(doc.createTextNode(buildInitHex(fontSchema, {
            {"enb", glyph.enabled ? 1u : 0u},
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
    fontEl.setAttribute("count", glyphIndex);

    for (auto it = sourceFont.kerningPairs.constBegin(); it != sourceFont.kerningPairs.constEnd(); ++it) {
        if (it.key().size() < 2)
            continue;

        const QChar left = it.key().at(0);
        const QChar right = it.key().at(1);
        if (!characters.contains(left) || !characters.contains(right))
            continue;

        font.kerningPairs.insert(it.key(), it.value());
        QDomElement kerningEl = doc.createElement("kerning");
        kerningEl.setAttribute("left", QString(left));
        kerningEl.setAttribute("right", QString(right));
        kerningEl.setAttribute("delta", it.value());
        fontEl.appendChild(kerningEl);
    }

    QDomElement dataEl = doc.createElement("data");
    dataEl.appendChild(doc.createTextNode(dataValues.join(", ")));
    fontEl.appendChild(dataEl);

    return fontEl;
}

} // namespace

// Сначала объявляются схемы и общие шрифты, затем объекты в аппаратном порядке.
// Порядок редактора front-to-back разворачивается при записи обычных объектов.
QDomDocument FpgaXmlExporter::compile(const EditorProjectDocument &document,
                                       const QList<QSharedPointer<BaseObject>> &objects,
                                       const QStringList &objectTags)
{
    const auto projectName = document.projectName();
    const int canvasWidth = document.canvasWidth();
    const int canvasHeight = document.canvasHeight();
    const auto backgroundColor = document.backgroundColor();
    QDomDocument doc;

    QDomElement root = doc.createElement("project");
    root.setAttribute("name", projectName.isEmpty() ? QStringLiteral("Untitled") : projectName);
    root.setAttribute("width", canvasWidth);
    root.setAttribute("height", canvasHeight);
    root.setAttribute("bgcolor", formatProjectColor(backgroundColor));
    doc.appendChild(root);

    appendParameterSchemas(doc, root, collectCompiledSchemaNames(objects, objectTags));

    QDomElement objectsEl = doc.createElement("objects");
    root.appendChild(objectsEl);

    auto *registry = FpgaSchemaRegistry::instance();
    // Экспорт использует ту же актуальную раскладку init, что записывается в <parameters>.
    // Импортированная схема нужна только для разбора исходного документа.
    const ParamSchema staticSchema = registry->buildSchema(QStringLiteral("staticgroup"));
    const ParamSchema rotationSchema = registry->buildSchema(QStringLiteral("rotationobject"));
    const ParamSchema fontSchema = registry->buildSchema(QStringLiteral("font"));
    const ParamSchema textSchema = registry->buildSchema(QStringLiteral("text"));

    const ParamSchema layer3dSchema = registry->buildSchema(QStringLiteral("layer3d"));
    for (const auto &obj : objects) {
        if (dynamic_cast<ThreeDViewObject*>(obj.data()) && obj->isExportEnabled()) {
            objectsEl.appendChild(createObjectElement(
                doc, QStringLiteral("layer3d"), layer3dSchema, obj));
            break;
        }
    }

    QMap<ExportFontKey, QList<TextObject*>> textByFont;
    for (const auto &obj : objects) {
        if (auto text = dynamic_cast<TextObject*>(obj.data())) {
            if (text->isExportEnabled())
                textByFont[exportFontKeyForText(text)].append(text);
        }
    }

    QMap<ExportFontKey, int> fontIndexByKey;
    int nextFontIndex = 0;
    for (auto it = textByFont.constBegin(); it != textByFont.constEnd(); ++it) {
        if (it.key().existingAtlas) {
            const FpgaFont *sourceFont = nullptr;
            for (TextObject *text : it.value()) {
                if (text && text->hasFontAtlas()) {
                    sourceFont = &text->fontAtlas();
                    break;
                }
            }
            if (!sourceFont)
                continue;

            QString characters = sourceFont->orderedCharacters();
            for (TextObject *text : it.value()) {
                for (const QChar ch : text->text) {
                    if (ch.isSpace() && !characters.contains(ch))
                        characters.append(ch);
                }
            }
            if (characters.isEmpty())
                continue;

            objectsEl.appendChild(createFontResourceElementFromAtlas(doc, *sourceFont, nextFontIndex, characters, fontSchema));
        } else {
            QString characters = completeAlphabetForTextObjects(it.value());
            for (TextObject *text : it.value()) {
                for (const QChar ch : text->exportCharacters()) {
                    if (!characters.contains(ch))
                        characters.append(ch);
                }
            }
            if (characters.isEmpty())
                characters = QStringLiteral(" ");

            objectsEl.appendChild(createFontResourceElement(doc, it.key(), nextFontIndex, characters, fontSchema));
        }
        fontIndexByKey.insert(it.key(), nextFontIndex);
        ++nextFontIndex;
    }

    bool firstScale = true;
    for (int objIdx = objects.size() - 1; objIdx >= 0; --objIdx) {
        const auto &obj = objects[objIdx];
        if (!obj->isExportEnabled())
            continue;

        if (dynamic_cast<ThreeDViewObject*>(obj.data()))
            continue;

        if (auto image = dynamic_cast<ImageObject*>(obj.data())) {
            appendCompiledImageElements(doc, objectsEl, staticSchema, rotationSchema, image);
            continue;
        }

        if (auto text = dynamic_cast<TextObject*>(obj.data())) {
            const ExportFontKey key = exportFontKeyForText(text);
            const int fontIndex = fontIndexByKey.value(key, 0);
            const GroupState state = text->states.isEmpty() ? GroupState{} : text->states.first();
            QStringList textDataCodes;
            for (const QChar ch : text->text)
                textDataCodes.append(QString::number(ch.unicode()));

            QDomElement textEl = doc.createElement("text");

            QDomElement initEl = doc.createElement("init");
            initEl.setAttribute("index", fontIndex);
            initEl.setAttribute("text", text->text);
            initEl.appendChild(doc.createTextNode(buildInitHex(textSchema, {
                {"enb", state.enabled ? 1u : 0u},
                {"color", BitParser::colorToBgr(state.color)},
                {"x", static_cast<quint32>(state.x)},
                {"y", static_cast<quint32>(state.y)},
                {"font_index", static_cast<quint32>(fontIndex)},
                {"char_offset", 0u},
                {"char_count", static_cast<quint32>(text->text.size())}
            })));
            textEl.appendChild(initEl);

            QDomElement dataEl = doc.createElement("data");
            dataEl.appendChild(doc.createTextNode(textDataCodes.join(", ")));
            textEl.appendChild(dataEl);

            objectsEl.appendChild(textEl);
            continue;
        }

        const QString tagName = objectTagForSerialization(objIdx, objects, objectTags);
        const QString schemaName = tagName;
        const ParamSchema schema = registry->buildSchema(schemaName);
        if (tagName.isEmpty() || schema.isEmpty())
            continue;
        if (tagName == QStringLiteral("RibonScale")) {
            auto scaleEl = doc.createElement(tagName);
            for (const auto &word : FpgaScalePlaceholder::parameters(firstScale)) {
                auto initEl = doc.createElement("init");
                initEl.appendChild(doc.createTextNode(word));
                scaleEl.appendChild(initEl);
            }
            objectsEl.appendChild(scaleEl);
            firstScale = false;
            continue;
        }
        objectsEl.appendChild(createObjectElement(doc, tagName, schema, obj));
    }

    return doc;
}
