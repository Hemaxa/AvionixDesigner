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
#include <QtMath>

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

void copyBaseFlags(const BaseObject *source, BaseObject *target)
{
    target->setViewVisible(source->isViewVisible());
    target->setExportEnabled(source->isExportEnabled());
    target->setImportedHardwareObject(source->isImportedHardwareObject());
    target->setResizeLocked(!source->canResize());
    target->setCustomName(source->customName());
}

QSharedPointer<BaseObject> cloneObject(const QSharedPointer<BaseObject> &source)
{
    if (!source)
        return {};

    BaseObject *rawClone = nullptr;

    if (auto rect = dynamic_cast<RectangleObject*>(source.data())) {
        auto *clone = new RectangleObject();
        clone->x = rect->x;
        clone->y = rect->y;
        clone->width = rect->width;
        clone->height = rect->height;
        clone->fillColor = rect->fillColor;
        clone->strokeColor = rect->strokeColor;
        clone->strokeWidth = rect->strokeWidth;
        clone->alpha = rect->alpha;
        clone->explicitAlpha = rect->explicitAlpha;
        rawClone = clone;
    } else if (auto dashed = dynamic_cast<DashedLineObject*>(source.data())) {
        auto *clone = new DashedLineObject();
        clone->enabled = dashed->enabled;
        clone->color = dashed->color;
        clone->x0 = dashed->x0;
        clone->y0 = dashed->y0;
        clone->x1 = dashed->x1;
        clone->y1 = dashed->y1;
        clone->dashPeriod = dashed->dashPeriod;
        clone->dashLength = dashed->dashLength;
        clone->dashPhase = dashed->dashPhase;
        clone->lineWidth = dashed->lineWidth;
        rawClone = clone;
    } else if (auto ribbon = dynamic_cast<RibbonScaleObject*>(source.data())) {
        auto *clone = new RibbonScaleObject();
        clone->enabled = ribbon->enabled;
        clone->color = ribbon->color;
        clone->left = ribbon->left;
        clone->right = ribbon->right;
        clone->top = ribbon->top;
        clone->bottom = ribbon->bottom;
        clone->lineWidth = ribbon->lineWidth;
        clone->period = ribbon->period;
        clone->yStart = ribbon->yStart;
        rawClone = clone;
    } else if (auto horizon = dynamic_cast<AviaHorizonObject*>(source.data())) {
        auto *clone = new AviaHorizonObject();
        clone->enabled = horizon->enabled;
        clone->earthColor = horizon->earthColor;
        clone->skyColor = horizon->skyColor;
        clone->horizonLineColor = horizon->horizonLineColor;
        clone->lineWidth = horizon->lineWidth;
        clone->xCenter = horizon->xCenter;
        clone->yCenter = horizon->yCenter;
        clone->areaWidth = horizon->areaWidth;
        clone->areaHeight = horizon->areaHeight;
        clone->sinVal = horizon->sinVal;
        clone->cosVal = horizon->cosVal;
        rawClone = clone;
    } else if (auto text = dynamic_cast<TextObject*>(source.data())) {
        auto *clone = new TextObject();
        clone->states = text->states;
        clone->groupNumber = text->groupNumber;
        clone->maskImages = text->maskImages;
        clone->text = text->text;
        clone->fontFamily = text->fontFamily;
        clone->pixelSize = text->pixelSize;
        clone->fontIndex = text->fontIndex;
        clone->restrictedAtlasEditing = text->restrictedAtlasEditing;
        clone->exportAlphabetGroups = text->exportAlphabetGroups;
        if (text->hasFontAtlas())
            clone->setFontAtlas(text->fontAtlas(), text->restrictedAtlasEditing);
        clone->extraCharacters = text->extraCharacters;
        rawClone = clone;
    } else if (auto group = dynamic_cast<StaticGroupObject*>(source.data())) {
        auto *clone = new StaticGroupObject();
        clone->states = group->states;
        clone->groupNumber = group->groupNumber;
        clone->maskImages = group->maskImages;
        rawClone = clone;
    } else if (auto rotation = dynamic_cast<RotationObject*>(source.data())) {
        auto *clone = new RotationObject();
        clone->left = rotation->left;
        clone->top = rotation->top;
        clone->right = rotation->right;
        clone->bottom = rotation->bottom;
        clone->xRot = rotation->xRot;
        clone->yRot = rotation->yRot;
        clone->sinVal = rotation->sinVal;
        clone->cosVal = rotation->cosVal;
        clone->color = rotation->color;
        clone->maskImage = rotation->maskImage;
        rawClone = clone;
    } else if (auto image = dynamic_cast<ImageObject*>(source.data())) {
        auto *clone = new ImageObject();
        clone->x = image->x;
        clone->y = image->y;
        clone->width = image->width;
        clone->height = image->height;
        clone->maskColor = image->maskColor;
        clone->autoMaskColor = image->autoMaskColor;
        clone->rotationDegrees = image->rotationDegrees;
        clone->sourceName = image->sourceName;
        clone->format = image->format;
        clone->setSourcePayload(image->sourcePayload());
        clone->setColorLayers(image->colorLayers());
        rawClone = clone;
    }

    if (!rawClone)
        return {};

    copyBaseFlags(source.data(), rawClone);
    return QSharedPointer<BaseObject>(rawClone);
}

QList<QSharedPointer<BaseObject>> cloneObjectList(const QList<QSharedPointer<BaseObject>> &objects)
{
    QList<QSharedPointer<BaseObject>> clones;
    clones.reserve(objects.size());
    for (const auto &object : objects)
        clones.append(cloneObject(object));
    return clones;
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

QString orderedFontCharacters(const FpgaFont &font)
{
    QString characters;
    for (const QChar ch : font.glyphOrder) {
        if (font.glyphs.contains(ch) && !characters.contains(ch))
            characters.append(ch);
    }
    for (auto it = font.glyphs.constBegin(); it != font.glyphs.constEnd(); ++it) {
        if (!characters.contains(it.key()))
            characters.append(it.key());
    }
    return characters;
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

    if (glyph.advance <= 0)
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

QStringList parseCharacterCodes(const QString &dataText)
{
    QStringList codes;
    for (const QString &part : dataText.split(',', Qt::SkipEmptyParts))
        codes.append(part.trimmed());
    return codes;
}

QString textFromCharacterCodes(const QStringList &codes, int offset, int count)
{
    QString value;
    if (offset < 0 || count <= 0 || offset >= codes.size())
        return value;

    const int end = qMin(codes.size(), offset + count);
    value.reserve(end - offset);
    for (int i = offset; i < end; ++i) {
        bool ok = false;
        const uint code = codes[i].toUInt(&ok);
        if (ok)
            value.append(QChar(static_cast<ushort>(code)));
    }
    return value;
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
                                         const QStringList &objectTags,
                                         const QMap<QString, QString> &schemaAliases)
{
    QSet<QString> schemaNames;
    auto *registry = FpgaSchemaRegistry::instance();

    for (int objIdx = objects.size() - 1; objIdx >= 0; --objIdx) {
        const auto &obj = objects[objIdx];
        if (!obj->isExportEnabled())
            continue;

        if (auto image = dynamic_cast<ImageObject*>(obj.data())) {
            schemaNames.insert(image->hasRotation() ? QStringLiteral("rotationobject") : QStringLiteral("staticgroup"));
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

        schemaNames.insert(schemaAliases.value(tagName, registry->canonicalSchemaName(tagName)));
    }

    return schemaNames;
}

QDomElement createStaticGroupElementFromImage(QDomDocument &doc, const ParamSchema &schema, const ImageObject *image)
{
    StaticGroupObject group;
    const QList<ImageMaskComponent> components = image->maskComponents();
    int nextAddr = 0;

    if (components.isEmpty()) {
        GroupState state;
        state.x = qRound(image->x);
        state.y = qRound(image->y);
        state.w = qMax(1, qRound(image->width));
        state.h = qMax(1, qRound(image->height));
        state.addr = 0;
        state.color = image->effectiveMaskColor();
        state.enabled = image->isViewVisible();
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
            state.enabled = image->isViewVisible();
            group.states.append(state);
            group.maskImages.append(component.mask);
            nextAddr += state.w * state.h;
        }
    }

    group.groupNumber = qMax(1, group.states.size());
    return createObjectElement(doc, QStringLiteral("staticgroup"), schema, QSharedPointer<BaseObject>(&group, [](BaseObject*){}));
}

QDomElement createRotationObjectElementFromImageComponent(QDomDocument &doc,
                                                         const ParamSchema &schema,
                                                         const ImageObject *image,
                                                         const ImageMaskComponent &component)
{
    RotationObject rotation;
    rotation.setViewVisible(image->isViewVisible());
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
    rotation.sinVal = qRound(qSin(qDegreesToRadians(image->rotationDegrees)) * 65536.0);
    rotation.cosVal = qRound(qCos(qDegreesToRadians(image->rotationDegrees)) * 65536.0);

    return createObjectElement(doc, QStringLiteral("rotationobject"), schema, QSharedPointer<BaseObject>(&rotation, [](BaseObject*){}));
}

void appendCompiledImageElements(QDomDocument &doc,
                                 QDomElement &objectsEl,
                                 const ParamSchema &staticSchema,
                                 const ParamSchema &rotationSchema,
                                 const ImageObject *image)
{
    if (!image->hasRotation()) {
        objectsEl.appendChild(createStaticGroupElementFromImage(doc, staticSchema, image));
        return;
    }

    const QList<ImageMaskComponent> components = image->maskComponents();
    if (components.isEmpty()) {
        ImageMaskComponent component;
        component.bounds = QRect(0, 0, qMax(1, qRound(image->width)), qMax(1, qRound(image->height)));
        component.color = image->effectiveMaskColor();
        component.mask = image->renderedImage();
        objectsEl.appendChild(createRotationObjectElementFromImageComponent(doc, rotationSchema, image, component));
        return;
    }

    for (const ImageMaskComponent &component : components)
        objectsEl.appendChild(createRotationObjectElementFromImageComponent(doc, rotationSchema, image, component));
}

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

QDomElement createFontResourceElementFromAtlas(QDomDocument &doc,
                                               const FpgaFont &sourceFont,
                                               int fontIndex,
                                               const QString &characters,
                                               const ParamSchema &fontSchema,
                                               FpgaFont *fontOut)
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
                glyph = normalizedGlyph(glyph, glyph.literal, font.size);
                insertGlyph(&font, glyph);
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
                glyph.advance = glyph.width;
                glyph.bearingY = glyph.floater;
                glyph.ascent = qMax(1, glyph.height + qMin(0, glyph.floater));
                const QStringList rows = glyphEl.text().split('\n', Qt::SkipEmptyParts);
                QStringList trimmedRows;
                for (const QString &row : rows) {
                    const QString trimmed = row.trimmed();
                    if (!trimmed.isEmpty())
                        trimmedRows.append(trimmed);
                }
                glyph.maskRows = trimmedRows.join('\n');
                glyph = normalizedGlyph(glyph, glyph.literal, font.size);
                insertGlyph(&font, glyph);
            }
        }
        glyphEl = glyphEl.nextSiblingElement();
    }

    return font;
}

QDomDocument buildCompiledXmlDocument(const QString &projectName,
                                      int canvasWidth,
                                      int canvasHeight,
                                      const QColor &backgroundColor,
                                      const QMap<QString, ParamSchema> &schemas,
                                      const QMap<QString, QString> &schemaAliases,
                                      const QList<QSharedPointer<BaseObject>> &objects,
                                      const QStringList &objectTags,
                                      QMap<int, FpgaFont> *fontsOut)
{
    QDomDocument doc;
    doc.appendChild(doc.createProcessingInstruction("xml", "version='1.0' encoding='windows-1251'"));

    QDomElement root = doc.createElement("project");
    root.setAttribute("name", projectName.isEmpty() ? QStringLiteral("Untitled") : projectName);
    root.setAttribute("width", canvasWidth);
    root.setAttribute("height", canvasHeight);
    root.setAttribute("bgcolor", formatProjectColor(backgroundColor));
    doc.appendChild(root);

    appendParameterSchemas(doc, root, collectCompiledSchemaNames(objects, objectTags, schemaAliases));

    QDomElement objectsEl = doc.createElement("objects");
    root.appendChild(objectsEl);

    auto *registry = FpgaSchemaRegistry::instance();
    const ParamSchema staticSchema = schemas.value(QStringLiteral("staticgroup"), registry->buildSchema(QStringLiteral("staticgroup")));
    const ParamSchema rotationSchema = schemas.value(QStringLiteral("rotationobject"), registry->buildSchema(QStringLiteral("rotationobject")));
    const ParamSchema fontSchema = registry->buildSchema(QStringLiteral("font"));
    const ParamSchema textSchema = registry->buildSchema(QStringLiteral("text"));

    QMap<ExportFontKey, QList<TextObject*>> textByFont;
    for (const auto &obj : objects) {
        if (auto text = dynamic_cast<TextObject*>(obj.data())) {
            if (text->isExportEnabled())
                textByFont[exportFontKeyForText(text)].append(text);
        }
    }

    QMap<ExportFontKey, int> fontIndexByKey;
    QMap<int, FpgaFont> generatedFonts;
    int nextFontIndex = 0;
    for (auto it = textByFont.constBegin(); it != textByFont.constEnd(); ++it) {
        FpgaFont font;
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

            QString characters = orderedFontCharacters(*sourceFont);
            for (TextObject *text : it.value()) {
                for (const QChar ch : text->text) {
                    if (ch.isSpace() && !characters.contains(ch))
                        characters.append(ch);
                }
            }
            if (characters.isEmpty())
                continue;

            objectsEl.appendChild(createFontResourceElementFromAtlas(doc, *sourceFont, nextFontIndex, characters, fontSchema, &font));
        } else {
            QString characters = completeAlphabetForTextObjects(it.value());
            for (TextObject *text : it.value()) {
                for (const QChar ch : text->exportCharacters()) {
                    if (!characters.contains(ch))
                        characters.append(ch);
                }
            }
            if (characters.isEmpty())
                continue;

            objectsEl.appendChild(createFontResourceElement(doc, it.key(), nextFontIndex, characters, fontSchema, &font));
        }
        fontIndexByKey.insert(it.key(), nextFontIndex);
        generatedFonts.insert(nextFontIndex, font);
        ++nextFontIndex;
    }

    for (int objIdx = objects.size() - 1; objIdx >= 0; --objIdx) {
        const auto &obj = objects[objIdx];
        if (!obj->isExportEnabled())
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
                {"enb", text->isViewVisible() ? 1u : 0u},
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
        const QString schemaName = schemaAliases.value(tagName, registry->canonicalSchemaName(tagName));
        if (tagName.isEmpty() || !schemas.contains(schemaName))
            continue;
        objectsEl.appendChild(createObjectElement(doc, tagName, schemas[schemaName], obj));
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
    m_groups.clear();
    m_nextGroupId = 1;
    m_fonts.clear();
    clearHistory();
    
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
    } else {
        for (const QString &schemaName : registry->defaultSchemaNames()) {
            if (!m_schemas.contains(schemaName)) {
                const ParamSchema schema = registry->buildSchema(schemaName);
                if (!schema.isEmpty())
                    m_schemas.insert(schemaName, schema);
            }
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
            image->rotationDegrees = objEl.attribute("rotation", "0").toDouble();
            image->format = objEl.attribute("format", "raster");
            image->sourceName = objEl.attribute("source_name");
            image->maskColor = QColor(objEl.attribute("mask_color", "#FFFFFF"));
            image->autoMaskColor = objEl.attribute("mask_color_auto", "1").toInt() != 0;
            image->setViewVisible(objEl.attribute("visible", "1").toInt() != 0);
            image->setExportEnabled(objEl.attribute("export", "1").toInt() != 0);
            image->setSourcePayload(QByteArray::fromBase64(objEl.firstChildElement("source").text().toLatin1()));
            QList<ImageColorLayer> layers;
            QDomElement layerEl = objEl.firstChildElement("color_layers").firstChildElement("layer");
            while (!layerEl.isNull()) {
                ImageColorLayer layer;
                layer.sourceColor = QColor(layerEl.attribute("source", "#FFFFFF"));
                layer.maskColor = QColor(layerEl.attribute("color", layer.sourceColor.name()));
                layer.autoMaskColor = layerEl.attribute("auto", "1").toInt() != 0;
                layers.append(layer);
                layerEl = layerEl.nextSiblingElement("layer");
            }
            if (!layers.isEmpty())
                image->setColorLayers(layers);
            m_objects.append(QSharedPointer<BaseObject>(image));
            m_objectTags.append(QStringLiteral("image"));
        }
        else if (tagName == QStringLiteral("text")) {
            const ParamSchema textSchema = registry->buildSchema(QStringLiteral("text"));
            const QStringList characterCodes = parseCharacterCodes(objEl.firstChildElement("data").text());
            QDomElement initEl = objEl.firstChildElement("init");
            while (!initEl.isNull()) {
                const QString hex = initEl.text().trimmed();
                const int fontIndex = BitParser::extract(hex, textSchema["font_index"].offset, textSchema["font_index"].size);
                const int charOffset = BitParser::extract(hex, textSchema["char_offset"].offset, textSchema["char_offset"].size);
                const int charCount = BitParser::extract(hex, textSchema["char_count"].offset, textSchema["char_count"].size);
                auto *textObject = new TextObject();
                const QString dataText = textFromCharacterCodes(characterCodes, charOffset, charCount);
                textObject->text = dataText.isEmpty() ? initEl.attribute("text") : dataText;
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
                
            }
        }
        
        objNode = objNode.nextSibling();
    }

    std::reverse(m_objects.begin(), m_objects.end());
    std::reverse(m_objectTags.begin(), m_objectTags.end());
    
    emit logMessage(tr("Загружено объектов: %1").arg(m_objects.size()));
    applyRestrictedMode();
    
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
    m_showGrid = false;
    m_snapToCanvas = true;
    m_snapToGrid = false;
    m_snapToObjects = false;

    m_objects.clear();
    m_objectTags.clear();
    m_groups.clear();
    m_nextGroupId = 1;
    clearHistory();
    m_schemaAliases = registry->defaultSchemaAliases();
    m_fonts.clear();

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

ProjectManager::ProjectSnapshot ProjectManager::captureSnapshot() const
{
    return {cloneObjectList(m_objects), m_objectTags, m_groups, m_nextGroupId};
}

void ProjectManager::restoreSnapshot(const ProjectSnapshot &snapshot)
{
    m_objects = cloneObjectList(snapshot.objects);
    m_objectTags = snapshot.objectTags;
    m_groups = snapshot.groups;
    m_nextGroupId = snapshot.nextGroupId;
}

void ProjectManager::recordHistory()
{
    m_undoStack.append(captureSnapshot());
    m_redoStack.clear();
    constexpr int maxHistoryDepth = 80;
    while (m_undoStack.size() > maxHistoryDepth)
        m_undoStack.removeFirst();
}

void ProjectManager::clearHistory()
{
    m_undoStack.clear();
    m_redoStack.clear();
    m_clipboardObjects.clear();
    m_clipboardTags.clear();
}

void ProjectManager::insertObjectAtFront(BaseObject *object, const QString &tagName)
{
    m_objects.prepend(QSharedPointer<BaseObject>(object));
    m_objectTags.prepend(tagName);
}

bool ProjectManager::undo()
{
    if (m_undoStack.isEmpty())
        return false;

    m_redoStack.append(captureSnapshot());
    const ProjectSnapshot snapshot = m_undoStack.takeLast();
    restoreSnapshot(snapshot);
    emit projectChanged();
    emit logMessage(tr("Отмена действия"));
    return true;
}

bool ProjectManager::redo()
{
    if (m_redoStack.isEmpty())
        return false;

    m_undoStack.append(captureSnapshot());
    const ProjectSnapshot snapshot = m_redoStack.takeLast();
    restoreSnapshot(snapshot);
    emit projectChanged();
    emit logMessage(tr("Повтор действия"));
    return true;
}

bool ProjectManager::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool ProjectManager::canRedo() const
{
    return !m_redoStack.isEmpty();
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

    const int offset = 24 * (m_objects.size() % 6);
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
        horizon->xCenter = m_document->canvasWidth() / 2.0;
        horizon->yCenter = m_document->canvasHeight() / 2.0;
        horizon->areaWidth = m_document->canvasWidth();
        horizon->areaHeight = m_document->canvasHeight();
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
        if (!m_fonts.isEmpty()) {
            const FpgaFont font = m_fonts.constBegin().value();
            const QString characters = orderedFontCharacters(font);
            textObject->text = characters.isEmpty() ? QStringLiteral(" ") : QString(characters.front());
            textObject->setFontAtlas(font, true);
        } else {
            textObject->pixelSize = 14;
            textObject->fontFamily = QStringLiteral("Arial");
        }
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

    recordHistory();
    insertObjectAtFront(rawObject, registry->canonicalObjectTag(typeName));

    emit projectChanged();
    emit logMessage(tr("Добавлен объект: %1").arg(typeName));
    return 0;
}

int ProjectManager::addObject(const QString &typeName, double x, double y)
{
    const int index = addObject(typeName);
    if (index < 0)
        return index;

    auto obj = m_objects[index];
    if (dynamic_cast<AviaHorizonObject*>(obj.data())) {
        emit projectChanged();
        return index;
    }

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
    return removeObjects(QList<int>{index});
}

bool ProjectManager::removeObjects(const QList<int> &indexes)
{
    QList<int> normalized;
    QSet<int> seen;
    for (int index : indexes) {
        if (index < 0 || index >= m_objects.size() || seen.contains(index))
            continue;
        seen.insert(index);
        normalized.append(index);
    }

    if (normalized.isEmpty())
        return false;

    recordHistory();
    std::sort(normalized.begin(), normalized.end(), std::greater<int>());
    for (int index : normalized) {
        m_objects.removeAt(index);
        if (index < m_objectTags.size())
            m_objectTags.removeAt(index);
    }

    for (ObjectGroup &group : m_groups) {
        QList<int> updatedMembers;
        for (int member : group.members) {
            if (seen.contains(member))
                continue;
            int shift = 0;
            for (int removed : normalized) {
                if (removed < member)
                    ++shift;
            }
            updatedMembers.append(member - shift);
        }
        group.members = updatedMembers;
    }
    m_groups.erase(std::remove_if(m_groups.begin(), m_groups.end(), [](const ObjectGroup &group) {
        return group.members.size() < 2;
    }), m_groups.end());

    emit projectChanged();
    emit logMessage(tr("Удалено объектов: %1").arg(normalized.size()));
    return true;
}

bool ProjectManager::reorderObjects(const QList<int> &order)
{
    if (order.size() != m_objects.size())
        return false;

    bool changed = false;
    for (int i = 0; i < order.size(); ++i) {
        if (order[i] != i) {
            changed = true;
            break;
        }
    }
    if (!changed)
        return true;

    QList<QSharedPointer<BaseObject>> reorderedObjects;
    QStringList reorderedTags;
    reorderedObjects.reserve(m_objects.size());
    reorderedTags.reserve(m_objectTags.size());

    QSet<int> seen;
    QMap<int, int> oldToNew;
    for (int index : order) {
        if (index < 0 || index >= m_objects.size() || seen.contains(index))
            return false;

        seen.insert(index);
        oldToNew.insert(index, reorderedObjects.size());
        reorderedObjects.append(m_objects[index]);
        reorderedTags.append(index < m_objectTags.size() ? m_objectTags[index] : QString());
    }

    recordHistory();
    m_objects = reorderedObjects;
    m_objectTags = reorderedTags;
    for (ObjectGroup &group : m_groups) {
        QList<int> updatedMembers;
        for (int member : group.members) {
            if (oldToNew.contains(member))
                updatedMembers.append(oldToNew.value(member));
        }
        std::sort(updatedMembers.begin(), updatedMembers.end());
        group.members = updatedMembers;
    }
    emit projectChanged();
    emit logMessage(tr("Изменён порядок слоёв объектов"));
    return true;
}

bool ProjectManager::sendObjectsToFront(const QList<int> &indexes)
{
    QSet<int> selected;
    for (int index : indexes) {
        if (index >= 0 && index < m_objects.size())
            selected.insert(index);
    }
    if (selected.isEmpty())
        return false;

    QList<int> order;
    for (int i = 0; i < m_objects.size(); ++i) {
        if (selected.contains(i))
            order.append(i);
    }
    for (int i = 0; i < m_objects.size(); ++i) {
        if (!selected.contains(i))
            order.append(i);
    }
    return reorderObjects(order);
}

bool ProjectManager::sendObjectsToBack(const QList<int> &indexes)
{
    QSet<int> selected;
    for (int index : indexes) {
        if (index >= 0 && index < m_objects.size())
            selected.insert(index);
    }
    if (selected.isEmpty())
        return false;

    QList<int> order;
    for (int i = 0; i < m_objects.size(); ++i) {
        if (!selected.contains(i))
            order.append(i);
    }
    for (int i = 0; i < m_objects.size(); ++i) {
        if (selected.contains(i))
            order.append(i);
    }
    return reorderObjects(order);
}

int ProjectManager::groupObjects(const QList<int> &indexes)
{
    QList<int> normalized;
    QSet<int> seen;
    for (int index : indexes) {
        if (index < 0 || index >= m_objects.size() || seen.contains(index))
            continue;
        seen.insert(index);
        normalized.append(index);
    }
    std::sort(normalized.begin(), normalized.end());
    if (normalized.size() < 2)
        return -1;

    recordHistory();
    for (ObjectGroup &group : m_groups) {
        QList<int> kept;
        for (int member : group.members) {
            if (!seen.contains(member))
                kept.append(member);
        }
        group.members = kept;
    }
    m_groups.erase(std::remove_if(m_groups.begin(), m_groups.end(), [](const ObjectGroup &group) {
        return group.members.size() < 2;
    }), m_groups.end());

    ObjectGroup group;
    group.id = m_nextGroupId++;
    group.name = tr("Группа %1").arg(group.id);
    group.members = normalized;
    m_groups.append(group);

    emit projectChanged();
    emit logMessage(tr("Создана группа: %1 объектов").arg(group.members.size()));
    return group.id;
}

bool ProjectManager::ungroupObjects(const QList<int> &indexes)
{
    QSet<int> selected;
    for (int index : indexes) {
        if (index >= 0 && index < m_objects.size())
            selected.insert(index);
    }
    if (selected.isEmpty())
        return false;

    QList<int> removeGroupIds;
    for (const ObjectGroup &group : m_groups) {
        for (int member : group.members) {
            if (selected.contains(member)) {
                removeGroupIds.append(group.id);
                break;
            }
        }
    }
    if (removeGroupIds.isEmpty())
        return false;

    recordHistory();
    const QSet<int> removeSet(removeGroupIds.begin(), removeGroupIds.end());
    m_groups.erase(std::remove_if(m_groups.begin(), m_groups.end(), [&removeSet](const ObjectGroup &group) {
        return removeSet.contains(group.id);
    }), m_groups.end());

    emit projectChanged();
    emit logMessage(tr("Группа разгруппирована"));
    return true;
}

QList<int> ProjectManager::groupMembersForObject(int index) const
{
    for (const ObjectGroup &group : m_groups) {
        if (group.members.contains(index))
            return group.members;
    }
    return {};
}

QList<int> ProjectManager::groupMembers(int groupId) const
{
    for (const ObjectGroup &group : m_groups) {
        if (group.id == groupId)
            return group.members;
    }
    return {};
}

QString ProjectManager::groupName(int groupId) const
{
    for (const ObjectGroup &group : m_groups) {
        if (group.id == groupId)
            return group.name;
    }
    return {};
}

bool ProjectManager::renameGroup(int groupId, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return false;

    for (ObjectGroup &group : m_groups) {
        if (group.id != groupId)
            continue;
        if (group.name == trimmed)
            return true;
        recordHistory();
        group.name = trimmed;
        emit projectChanged();
        return true;
    }
    return false;
}

bool ProjectManager::renameObject(int index, const QString &name)
{
    if (index < 0 || index >= m_objects.size())
        return false;

    const QString trimmed = name.trimmed();
    if (m_objects[index]->customName() == trimmed)
        return true;

    recordHistory();
    m_objects[index]->setCustomName(trimmed);
    emit projectChanged();
    return true;
}

bool ProjectManager::copyObjects(const QList<int> &indexes)
{
    m_clipboardObjects.clear();
    m_clipboardTags.clear();

    QSet<int> seen;
    for (int index : indexes) {
        if (index < 0 || index >= m_objects.size() || seen.contains(index))
            continue;
        seen.insert(index);
        m_clipboardObjects.append(cloneObject(m_objects[index]));
        m_clipboardTags.append(index < m_objectTags.size() ? m_objectTags[index] : QString());
    }

    if (m_clipboardObjects.isEmpty())
        return false;

    emit logMessage(tr("Скопировано объектов: %1").arg(m_clipboardObjects.size()));
    return true;
}

QList<int> ProjectManager::pasteObjects()
{
    QList<int> pastedIndexes;
    if (m_clipboardObjects.isEmpty())
        return pastedIndexes;

    recordHistory();
    for (int i = m_clipboardObjects.size() - 1; i >= 0; --i) {
        const auto clone = cloneObject(m_clipboardObjects[i]);
        if (!clone)
            continue;
        clone->moveBy(24.0, 24.0);
        m_objects.prepend(clone);
        m_objectTags.prepend(i < m_clipboardTags.size() ? m_clipboardTags[i] : QString());
    }

    for (int i = 0; i < m_clipboardObjects.size(); ++i)
        pastedIndexes.append(i);

    emit projectChanged();
    emit logMessage(tr("Вставлено объектов: %1").arg(pastedIndexes.size()));
    return pastedIndexes;
}

bool ProjectManager::canPasteObjects() const
{
    return !m_clipboardObjects.isEmpty();
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

    recordHistory();
    object->moveBy(dx, dy);
    emit projectChanged();
    emit logMessage(tr("Выполнено выравнивание объекта"));
    return true;
}

bool ProjectManager::setObjectViewVisible(int index, bool visible)
{
    if (index < 0 || index >= m_objects.size())
        return false;

    if (m_objects[index]->isViewVisible() == visible)
        return true;

    recordHistory();
    m_objects[index]->setViewVisible(visible);
    emit projectChanged();
    return true;
}

bool ProjectManager::setObjectExportEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_objects.size())
        return false;

    if (m_objects[index]->isExportEnabled() == enabled)
        return true;

    recordHistory();
    m_objects[index]->setExportEnabled(enabled);
    emit projectChanged();
    return true;
}

void ProjectManager::recordObjectEdit()
{
    recordHistory();
}

void ProjectManager::finishObjectEdit(const QString &message)
{
    emit projectChanged();
    if (!message.isEmpty())
        emit logMessage(message);
}

bool ProjectManager::setObjectProperty(BaseObject *object, const QString &propertyName, const QString &value)
{
    if (!object)
        return false;

    bool objectBelongsToProject = false;
    for (const auto &projectObject : m_objects) {
        if (projectObject.data() == object) {
            objectBelongsToProject = true;
            break;
        }
    }
    if (!objectBelongsToProject)
        return false;

    for (const auto &property : object->getProperties()) {
        if (property.first == propertyName) {
            if (property.second == value)
                return true;
            break;
        }
    }

    const ProjectSnapshot before = captureSnapshot();
    if (!object->setObjectProperty(propertyName, value))
        return false;

    m_undoStack.append(before);
    m_redoStack.clear();
    constexpr int maxHistoryDepth = 80;
    while (m_undoStack.size() > maxHistoryDepth)
        m_undoStack.removeFirst();

    emit projectChanged();
    return true;
}

bool ProjectManager::saveToFile(const QString &targetFile)
{
    return exportToFpgaXml(targetFile);
}

bool ProjectManager::exportToFpgaXml(const QString &targetFile)
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
        m_schemas,
        m_schemaAliases,
        m_objects,
        m_objectTags,
        &generatedFonts
    );
    
    //записываем XML обратно в файл
    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    
    QString xmlText = doc.toString(4);
    xmlText.replace(QStringLiteral("α"), QStringLiteral("&#945;"));
    QStringEncoder encoder("windows-1251");
    QByteArray encodedXml = encoder(xmlText);
    if (encodedXml.isEmpty() && !xmlText.isEmpty()) {
        encodedXml = xmlText.toUtf8();
    }
    outFile.write(encodedXml);
    outFile.close();

    emit logMessage(tr("Кадр для ПЛИС сохранён: %1").arg(outPath));
    return loadXmlProject(outPath);
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

    recordHistory();
    insertObjectAtFront(imageObject, QStringLiteral("image"));
    emit projectChanged();
    emit logMessage(tr("Изображение добавлено: %1").arg(fileName));
    return 0;
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
QList<ProjectManager::ObjectGroup> ProjectManager::objectGroups() const { return m_groups; }
bool ProjectManager::showGrid() const { return m_showGrid; }
bool ProjectManager::snapToCanvas() const { return m_snapToCanvas; }
bool ProjectManager::snapToGrid() const { return m_snapToGrid; }
bool ProjectManager::snapToObjects() const { return m_snapToObjects; }

int ProjectManager::gridStep() const { return m_gridStep; }
QColor ProjectManager::gridColor() const { return m_gridColor; }
QColor ProjectManager::snapCanvasGuideColor() const { return m_snapCanvasGuideColor; }
QColor ProjectManager::snapGridGuideColor() const { return m_snapGridGuideColor; }
QColor ProjectManager::snapObjectGuideColor() const { return m_snapObjectGuideColor; }

void ProjectManager::reloadGlobalSettings()
{
    QSettings settings("Avionix", "Designer");
    m_gridStep = settings.value("gridStep", 10).toInt();
    
    QString colorStr = settings.value("gridColor", "#3778b4c8").toString();
    m_gridColor = QColor(colorStr);
    m_snapCanvasGuideColor = QColor(settings.value("snapCanvasGuideColor", "#d2ff5c7a").toString());
    m_snapGridGuideColor = QColor(settings.value("snapGridGuideColor", "#d256d3ff").toString());
    m_snapObjectGuideColor = QColor(settings.value("snapObjectGuideColor", "#dcffca58").toString());
    
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
