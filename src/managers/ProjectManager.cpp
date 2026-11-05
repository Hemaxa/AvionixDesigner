// ProjectManager - менеджер проекта, связывающий внутреннюю модель редактора с экспортом в XML.

#include <QFile>
#include <QDomDocument>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QImageReader>
#include <QSvgRenderer>
#include "ApplicationSettings.h"
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
#include "ThreeDViewObject.h"
#include <QScopedValueRollback>
#include "TextObject.h"
#include "text/FontRasterizer.h"
#include "fpga/model/FpgaCompiledDocument.h"
#include "fpga/compiler/FpgaResourceLimits.h"
#include "RasterGeometry.h"
#include "BitParser.h"

#include <algorithm>
#include <limits>

namespace {
constexpr int kFallbackStaticGroupMaxDimension = 4095;
constexpr int kFallbackStaticGroupAddressablePixels = 65536;
constexpr int kFallbackRotationObjectAddressableTiles = 2048;

void moveObjectInsideCanvas(BaseObject *object, int canvasW, int canvasH)
{
    if (!object || canvasW <= 0 || canvasH <= 0)
        return;

    const QRectF bounds = object->getBoundingRect();
    double dx = 0.0;
    double dy = 0.0;

    if (bounds.width() <= canvasW) {
        if (bounds.left() < 0.0)
            dx = -bounds.left();
        else if (bounds.right() > canvasW)
            dx = canvasW - bounds.right();
    } else {
        dx = -bounds.left();
    }

    if (bounds.height() <= canvasH) {
        if (bounds.top() < 0.0)
            dy = -bounds.top();
        else if (bounds.bottom() > canvasH)
            dy = canvasH - bounds.bottom();
    } else {
        dy = -bounds.top();
    }

    if (!qFuzzyIsNull(dx) || !qFuzzyIsNull(dy))
        object->moveBy(dx, dy);
}

QString compactEncodingName(const QString &encoding)
{
    QString result = encoding.trimmed().toLower();
    result.remove(QLatin1Char('-'));
    result.remove(QLatin1Char('_'));
    result.remove(QLatin1Char(' '));
    return result;
}

bool isWindows1251Encoding(const QString &encoding)
{
    const QString compact = compactEncodingName(encoding);
    return compact == QStringLiteral("windows1251")
        || compact == QStringLiteral("cp1251")
        || compact == QStringLiteral("mscp1251")
        || compact == QStringLiteral("microsoftcp1251")
        || compact == QStringLiteral("xcp1251");
}

QString declaredXmlEncoding(const QByteArray &bytes)
{
    const QString header = QString::fromLatin1(bytes.left(1024));
    const QRegularExpression encodingRe(
        QStringLiteral("<\\?xml[^>]*\\bencoding\\s*=\\s*(['\\\"])([^'\\\"]+)\\1"),
        QRegularExpression::CaseInsensitiveOption
    );
    const QRegularExpressionMatch match = encodingRe.match(header);
    return match.hasMatch() ? match.captured(2).trimmed() : QString();
}

QString normalizeXmlDeclarationEncoding(QString text)
{
    const int declStart = text.indexOf(QStringLiteral("<?xml"), 0, Qt::CaseInsensitive);
    if (declStart < 0)
        return text;
    if (!text.left(declStart).trimmed().isEmpty())
        return text;

    const int declEnd = text.indexOf(QStringLiteral("?>"), declStart);
    if (declEnd < 0)
        return text;

    QString declaration = text.mid(declStart, declEnd + 2 - declStart);
    const QRegularExpression encodingRe(
        QStringLiteral("\\s+encoding\\s*=\\s*['\\\"][^'\\\"]+['\\\"]"),
        QRegularExpression::CaseInsensitiveOption
    );
    declaration.replace(encodingRe, QStringLiteral(" encoding=\"UTF-8\""));
    text.replace(declStart, declEnd + 2 - declStart, declaration);
    return text;
}

QString decodeWindows1251(const QByteArray &bytes)
{
    static constexpr ushort table80ToBF[] = {
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
        0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
        0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
        0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
        0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
        0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
        0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457
    };

    QString text;
    text.reserve(bytes.size());
    for (const char rawByte : bytes) {
        const uchar byte = static_cast<uchar>(rawByte);
        if (byte < 0x80) {
            text.append(QChar(byte));
        } else if (byte >= 0xC0) {
            text.append(QChar(static_cast<ushort>(byte < 0xE0
                ? 0x0410 + byte - 0xC0
                : 0x0430 + byte - 0xE0)));
        } else {
            text.append(QChar(table80ToBF[byte - 0x80]));
        }
    }
    return text;
}

QString decodeXmlWithQt(const QByteArray &bytes, QStringConverter::Encoding encoding, const QString &encodingName, QString *errorMessage)
{
    QStringDecoder decoder(encoding, QStringConverter::Flag::ConvertInitialBom);
    const QString text = decoder.decode(bytes);
    if (decoder.hasError()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Не удалось декодировать XML как %1").arg(encodingName);
        return QString();
    }
    return text;
}

QString decodeXmlWithNamedQtCodec(const QByteArray &bytes, const QString &encodingName, QString *errorMessage)
{
    const QByteArray codecName = encodingName.toLatin1();
    QStringDecoder decoder(codecName.constData(), QStringConverter::Flag::ConvertInitialBom);
    if (!decoder.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Кодировка XML \"%1\" не поддерживается этой сборкой Qt").arg(encodingName);
        return QString();
    }

    const QString text = decoder.decode(bytes);
    if (decoder.hasError()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Не удалось декодировать XML как %1").arg(encodingName);
        return QString();
    }
    return text;
}

QString decodeXmlBytes(const QByteArray &bytes, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    const QString declaredEncoding = declaredXmlEncoding(bytes);
    if (isWindows1251Encoding(declaredEncoding))
        return decodeWindows1251(bytes);

    if (!declaredEncoding.isEmpty())
        return decodeXmlWithNamedQtCodec(bytes, declaredEncoding, errorMessage);

    const auto detectedEncoding = QStringConverter::encodingForData(bytes);
    if (detectedEncoding)
        return decodeXmlWithQt(bytes, *detectedEncoding, QString::fromLatin1(QStringConverter::nameForEncoding(*detectedEncoding)), errorMessage);

    return decodeXmlWithQt(bytes, QStringConverter::Utf8, QStringLiteral("UTF-8"), errorMessage);
}

bool readXmlDocument(const QString &fileName, QDomDocument *doc, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Не удалось открыть файл: %1").arg(file.errorString());
        return false;
    }

    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Не удалось прочитать файл: %1").arg(file.errorString());
        return false;
    }

    QString decodeError;
    QString text = decodeXmlBytes(bytes, &decodeError);
    if (!decodeError.isEmpty()) {
        if (errorMessage)
            *errorMessage = decodeError;
        return false;
    }

    text = normalizeXmlDeclarationEncoding(text);
    const QDomDocument::ParseResult result = doc->setContent(text);
    if (!result) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Ошибка XML: %1 (строка %2, столбец %3)")
                .arg(result.errorMessage)
                .arg(result.errorLine)
                .arg(result.errorColumn);
        }
        return false;
    }

    return true;
}

int windows1251ByteForCodePoint(uint codePoint)
{
    if (codePoint <= 0x7F)
        return static_cast<int>(codePoint);
    if (codePoint >= 0x0410 && codePoint <= 0x042F)
        return 0xC0 + static_cast<int>(codePoint - 0x0410);
    if (codePoint >= 0x0430 && codePoint <= 0x044F)
        return 0xE0 + static_cast<int>(codePoint - 0x0430);

    switch (codePoint) {
    case 0x0402: return 0x80;
    case 0x0403: return 0x81;
    case 0x201A: return 0x82;
    case 0x0453: return 0x83;
    case 0x201E: return 0x84;
    case 0x2026: return 0x85;
    case 0x2020: return 0x86;
    case 0x2021: return 0x87;
    case 0x20AC: return 0x88;
    case 0x2030: return 0x89;
    case 0x0409: return 0x8A;
    case 0x2039: return 0x8B;
    case 0x040A: return 0x8C;
    case 0x040C: return 0x8D;
    case 0x040B: return 0x8E;
    case 0x040F: return 0x8F;
    case 0x0452: return 0x90;
    case 0x2018: return 0x91;
    case 0x2019: return 0x92;
    case 0x201C: return 0x93;
    case 0x201D: return 0x94;
    case 0x2022: return 0x95;
    case 0x2013: return 0x96;
    case 0x2014: return 0x97;
    case 0x2122: return 0x99;
    case 0x0459: return 0x9A;
    case 0x203A: return 0x9B;
    case 0x045A: return 0x9C;
    case 0x045C: return 0x9D;
    case 0x045B: return 0x9E;
    case 0x045F: return 0x9F;
    case 0x00A0: return 0xA0;
    case 0x040E: return 0xA1;
    case 0x045E: return 0xA2;
    case 0x0408: return 0xA3;
    case 0x00A4: return 0xA4;
    case 0x0490: return 0xA5;
    case 0x00A6: return 0xA6;
    case 0x00A7: return 0xA7;
    case 0x0401: return 0xA8;
    case 0x00A9: return 0xA9;
    case 0x0404: return 0xAA;
    case 0x00AB: return 0xAB;
    case 0x00AC: return 0xAC;
    case 0x00AD: return 0xAD;
    case 0x00AE: return 0xAE;
    case 0x0407: return 0xAF;
    case 0x00B0: return 0xB0;
    case 0x00B1: return 0xB1;
    case 0x0406: return 0xB2;
    case 0x0456: return 0xB3;
    case 0x0491: return 0xB4;
    case 0x00B5: return 0xB5;
    case 0x00B6: return 0xB6;
    case 0x00B7: return 0xB7;
    case 0x0451: return 0xB8;
    case 0x2116: return 0xB9;
    case 0x0454: return 0xBA;
    case 0x00BB: return 0xBB;
    case 0x0458: return 0xBC;
    case 0x0405: return 0xBD;
    case 0x0455: return 0xBE;
    case 0x0457: return 0xBF;
    default: return -1;
    }
}

QByteArray encodeXmlAsWindows1251(const QString &text)
{
    QByteArray encoded;
    encoded.reserve(text.size());

    for (int i = 0; i < text.size(); ++i) {
        uint codePoint = text.at(i).unicode();
        if (text.at(i).isHighSurrogate() && i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) {
            codePoint = QChar::surrogateToUcs4(text.at(i), text.at(i + 1));
            ++i;
        }

        const int byte = windows1251ByteForCodePoint(codePoint);
        if (byte >= 0) {
            encoded.append(static_cast<char>(byte));
        } else {
            encoded.append("&#");
            encoded.append(QByteArray::number(codePoint));
            encoded.append(';');
        }
    }

    return encoded;
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

int maxUnsignedFieldValue(const ParamSchema &schema, const QString &fieldName, int fallback)
{
    if (!schema.contains(fieldName))
        return fallback;

    const int size = schema[fieldName].size;
    if (size <= 0)
        return fallback;

    const qint64 value = size >= 31
        ? static_cast<qint64>(std::numeric_limits<int>::max())
        : ((static_cast<qint64>(1) << size) - 1);
    return static_cast<int>(qMin<qint64>(value, std::numeric_limits<int>::max()));
}

int staticGroupAddressablePixels(const ParamSchema &schema)
{
    const int maxAddr = maxUnsignedFieldValue(schema, QStringLiteral("addr"), kFallbackStaticGroupAddressablePixels - 1);
    return qMax(1, maxAddr + 1);
}

QSizeF fitSizeToStaticGroupLimits(const QSizeF &size, const ParamSchema &schema)
{
    if (size.width() <= 0.0 || size.height() <= 0.0)
        return QSizeF(1.0, 1.0);

    const int maxWidth = maxUnsignedFieldValue(schema, QStringLiteral("w"), kFallbackStaticGroupMaxDimension);
    const int maxHeight = maxUnsignedFieldValue(schema, QStringLiteral("h"), kFallbackStaticGroupMaxDimension);
    const int maxPixels = staticGroupAddressablePixels(schema);

    double scale = 1.0;
    if (size.width() > maxWidth)
        scale = qMin(scale, static_cast<double>(maxWidth) / size.width());
    if (size.height() > maxHeight)
        scale = qMin(scale, static_cast<double>(maxHeight) / size.height());
    if (size.width() * size.height() > maxPixels)
        scale = qMin(scale, qSqrt(static_cast<double>(maxPixels) / (size.width() * size.height())));

    return QSizeF(qMax(1.0, static_cast<double>(qFloor(size.width() * scale))),
                  qMax(1.0, static_cast<double>(qFloor(size.height() * scale))));
}

qint64 staticGroupPixelCost(const QList<ImageMaskComponent> &components)
{
    qint64 pixels = 0;
    for (const ImageMaskComponent &component : components)
        pixels += qMax<qint64>(1, static_cast<qint64>(component.bounds.width()) * component.bounds.height());
    return pixels;
}

QList<ImageMaskComponent> scaledStaticGroupComponents(const QList<ImageMaskComponent> &components,
                                                      double scale)
{
    QList<ImageMaskComponent> scaled;
    scaled.reserve(components.size());

    for (const ImageMaskComponent &component : components) {
        if (component.mask.isNull() || component.bounds.isEmpty())
            continue;

        ImageMaskComponent out;
        out.layerIndex = component.layerIndex;
        out.color = component.color;
        const int width = qMax(1, static_cast<int>(qFloor(component.bounds.width() * scale)));
        const int height = qMax(1, static_cast<int>(qFloor(component.bounds.height() * scale)));
        out.bounds = QRect(qRound(component.bounds.left() * scale),
                           qRound(component.bounds.top() * scale),
                           width,
                           height);
        out.mask = component.mask.scaled(width, height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        scaled.append(out);
    }

    return scaled;
}

QList<ImageMaskComponent> simplifyComponentsForStaticGroup(const ParamSchema &schema,
                                                           const QList<ImageMaskComponent> &components)
{
    const int maxWidth = maxUnsignedFieldValue(schema, QStringLiteral("w"), kFallbackStaticGroupMaxDimension);
    const int maxHeight = maxUnsignedFieldValue(schema, QStringLiteral("h"), kFallbackStaticGroupMaxDimension);
    const int maxPixels = staticGroupAddressablePixels(schema);
    if (components.isEmpty())
        return {};

    double scale = 1.0;
    for (const ImageMaskComponent &component : components) {
        if (component.bounds.width() > maxWidth)
            scale = qMin(scale, static_cast<double>(maxWidth) / component.bounds.width());
        if (component.bounds.height() > maxHeight)
            scale = qMin(scale, static_cast<double>(maxHeight) / component.bounds.height());
    }

    const qint64 initialPixels = staticGroupPixelCost(components);
    if (initialPixels > maxPixels)
        scale = qMin(scale, qSqrt(static_cast<double>(maxPixels) / initialPixels));

    if (scale >= 0.999)
        return components;

    QList<ImageMaskComponent> simplified = scaledStaticGroupComponents(components, scale);
    for (int attempt = 0; attempt < 8 && staticGroupPixelCost(simplified) > maxPixels; ++attempt) {
        const qint64 currentPixels = staticGroupPixelCost(simplified);
        scale *= qSqrt(static_cast<double>(maxPixels) / currentPixels) * 0.98;
        simplified = scaledStaticGroupComponents(components, scale);
    }

    return simplified;
}

QRect alphaBounds(const QImage &image)
{
    return rasterAlphaBounds(image);
}

int rotationTileCost(const QSize &size)
{
    if (size.width() <= 0 || size.height() <= 0)
        return 0;
    return ((size.width() + 7) / 8) * ((size.height() + 7) / 8);
}

ImageMaskComponent singleComponentForRotationObject(const ImageObject *image)
{
    ImageMaskComponent component;
    if (!image)
        return component;

    const QImage source = image->renderedSourceImage().convertToFormat(QImage::Format_ARGB32);
    if (source.isNull())
        return component;

    QRect bounds = alphaBounds(source);
    if (bounds.isEmpty())
        bounds = QRect(0, 0, qMax(1, source.width()), qMax(1, source.height()));

    component.bounds = bounds;
    component.color = image->effectiveMaskColor();
    component.mask = source.copy(bounds);
    return component;
}

ImageMaskComponent simplifyComponentForRotationObject(const ImageMaskComponent &component)
{
    if (component.mask.isNull() || component.bounds.isEmpty())
        return component;

    double scale = 1.0;
    if ((component.bounds.width() + 7) / 8 > 255)
        scale = qMin(scale, (255.0 * 8.0) / component.bounds.width());

    const int initialTiles = rotationTileCost(component.bounds.size());
    if (initialTiles > kFallbackRotationObjectAddressableTiles)
        scale = qMin(scale, qSqrt(static_cast<double>(kFallbackRotationObjectAddressableTiles) / initialTiles));

    if (scale >= 0.999)
        return component;

    ImageMaskComponent simplified = component;
    for (int attempt = 0; attempt < 8; ++attempt) {
        const int width = qMax(1, static_cast<int>(qFloor(component.bounds.width() * scale)));
        const int height = qMax(1, static_cast<int>(qFloor(component.bounds.height() * scale)));
        simplified.bounds = QRect(qRound(component.bounds.left() * scale),
                                  qRound(component.bounds.top() * scale),
                                  width,
                                  height);
        simplified.mask = component.mask.scaled(width, height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        if (rotationTileCost(simplified.bounds.size()) <= kFallbackRotationObjectAddressableTiles
            && (simplified.bounds.width() + 7) / 8 <= 255) {
            break;
        }
        scale *= 0.96;
    }

    return simplified;
}

void copyBaseFlags(const BaseObject *source, BaseObject *target)
{
    target->setViewVisible(source->isViewVisible());
    target->setSelectionLocked(source->isSelectionLocked());
    target->setExportEnabled(source->isExportEnabled());
    target->setImportedHardwareObject(source->isImportedHardwareObject());
    target->setResizeLocked(!source->canResize());
    target->setCustomName(source->customName());
    target->setEditorLayerId(source->editorLayerId());
}

QSharedPointer<BaseObject> cloneObject(const QSharedPointer<BaseObject> &source)
{
    if (!source)
        return {};

    BaseObject *rawClone = nullptr;

    if (auto rect = dynamic_cast<RectangleObject*>(source.data())) {
        auto *clone = new RectangleObject();
        clone->enabled = rect->enabled;
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
    } else if (auto threeDView = dynamic_cast<ThreeDViewObject*>(source.data())) {
        auto *clone = new ThreeDViewObject();
        clone->parse(threeDView->serializeInit(), FpgaSchemaRegistry::instance()->buildSchema(QStringLiteral("layer3d")));
        clone->setLeftAnchored(threeDView->isLeftAnchored());
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
        clone->enabled = rotation->enabled;
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
        if (!rotation->hardwareLayers().isEmpty()) {
            ParamSchema schema = FpgaSchemaRegistry::instance()->buildSchema(QStringLiteral("rotationobject"));
            QStringList initHexList;
            for (const auto &params : rotation->serializeLayerParams())
                initHexList.append(buildInitHex(schema, params));
            clone->setHardwareLayersFromInitHex(initHexList, schema);
        }
        rawClone = clone;
    } else if (auto image = dynamic_cast<ImageObject*>(source.data())) {
        auto *clone = new ImageObject(image->isRotationObject() ? ImageObject::Kind::Rotating : ImageObject::Kind::Static);
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

    const QList<ImageMaskComponent> simplifiedComponents = simplifyComponentsForStaticGroup(schema, components);
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
    if (!image->isRotationObject()) {
        appendStaticGroupElementsFromImageLayers(doc, objectsEl, staticSchema, image);
        return;
    }

    ImageMaskComponent component = singleComponentForRotationObject(image);
    if (component.mask.isNull() || component.bounds.isEmpty()) {
        component.bounds = QRect(0, 0, qMax(1, qRound(image->width)), qMax(1, qRound(image->height)));
        component.color = image->effectiveMaskColor();
        component.mask = image->renderedImage();
    }

    objectsEl.appendChild(createRotationObjectElementFromImageComponent(doc,
                                                                        rotationSchema,
                                                                        image,
                                                                        simplifyComponentForRotationObject(component)));
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

    if (fontOut)
        *fontOut = font;

    return fontEl;
}

QDomDocument buildCompiledXmlDocument(const QString &projectName,
                                      int canvasWidth,
                                      int canvasHeight,
                                      const QColor &backgroundColor,
                                      const QList<QSharedPointer<BaseObject>> &objects,
                                      const QStringList &objectTags,
                                      QMap<int, FpgaFont> *fontsOut)
{
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
        objectsEl.appendChild(createObjectElement(doc, tagName, schema, obj));
    }

    if (fontsOut)
        *fontsOut = generatedFonts;

    return doc;
}
}

ProjectManager::ProjectManager() : m_document(new EditorProjectDocument())
{
    resetEditorLayers();
    connect(this, &ProjectManager::projectLoaded, this, [this]() { ++m_sceneRevision; });
    connect(this, &ProjectManager::projectChanged, this, [this]() {
        if (!m_aircraftStateChange) ++m_sceneRevision;
    });
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

QString ProjectManager::lastErrorMessage() const
{
    return m_lastErrorMessage;
}

bool ProjectManager::loadXmlProject(const QString &fileName)
{
    emit logMessage(tr("Загрузка: %1").arg(fileName));

    QDomDocument doc;
    QString readError;
    if (!readXmlDocument(fileName, &doc, &readError)) {
        m_lastErrorMessage = readError;
        emit logMessage(tr("ОШИБКА: %1").arg(readError));
        return false;
    }

    auto *registry = FpgaSchemaRegistry::instance();
    const auto sourceDocument = FpgaCompiledDocument::fromDomDocument(doc, &m_lastErrorMessage);
    if (!sourceDocument.isValid()) {
        emit logMessage(tr("ОШИБКА: %1").arg(m_lastErrorMessage));
        return false;
    }
    m_lastErrorMessage.clear();
    m_filePath = fileName;
    m_objects.clear();
    m_objectTags.clear();
    m_groups.clear();
    m_nextGroupId = 1;
    resetEditorLayers();
    m_fonts.clear();
    clearHistory();

    // получаем корневой элемент
    QDomElement root = doc.documentElement();
    // читаем метаданные проекта
    m_document->setProjectName(root.attribute("name", "Untitled"));
    m_document->setCanvasSize(root.attribute("width", "640").toInt(), root.attribute("height", "480").toInt());
    m_document->setBackgroundColor(Qt::black);
    reloadGlobalSettings();

    // парсим цвет фона
    QString bgStr = root.attribute("bgcolor", "#0");
    if (bgStr.startsWith("#")) {
        bool ok;
        uint colorVal = bgStr.mid(1).toUInt(&ok, 16);
        if (ok) {
            m_document->setBackgroundColor(QColor(colorVal & 0xFF, (colorVal >> 8) & 0xFF, (colorVal >> 16) & 0xFF));
        }
    }

    emit logMessage(tr("Проект: %1 (%2x%3)").arg(m_document->projectName()).arg(m_document->canvasWidth()).arg(m_document->canvasHeight()));

    // парсим схемы параметров и сохраняем для использования при сохранении
    QDomElement paramsEl = root.firstChildElement("parameters");
    m_schemas = ObjectsManager::instance()->parseSchemas(paramsEl);
    QDomElement objectsEl = root.firstChildElement("objects");
    m_fonts = sourceDocument.fonts;
    for (const QString &type : m_schemas.keys()) {
        emit logMessage(tr("Схема: %1 (%2 полей)").arg(type).arg(m_schemas[type].size()));
    }

    // парсим объекты
    QDomNode objNode = objectsEl.firstChild();

    while (!objNode.isNull()) {
        QDomElement objEl = objNode.toElement();
        QString tagName = objEl.tagName();

        const QString schemaName = tagName;
        if (tagName == QStringLiteral("text")) {
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
                textObject->text = dataText;
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
            // Ресурсы шрифтов разбираются до создания объектов и не входят в список объектов сцены.
        }
        else if (m_schemas.contains(schemaName)) {
            QStringList initHexList;
            QDomElement initReaderEl = objEl.firstChildElement("init");
            while (!initReaderEl.isNull()) {
                const QString hex = initReaderEl.text().trimmed();
                if (!hex.isEmpty())
                    initHexList.append(hex);
                initReaderEl = initReaderEl.nextSiblingElement("init");
            }
            QString hexInit = initHexList.isEmpty() ? QString() : initHexList.first();

            // создаем объект через фабрику
            BaseObject *obj = ObjectsManager::instance()->createObject(tagName);

            if (obj && !hexInit.isEmpty()) {
                obj->parse(hexInit, m_schemas[schemaName]);
                if (auto rotation = dynamic_cast<RotationObject*>(obj))
                    rotation->setHardwareLayersFromInitHex(initHexList, m_schemas[schemaName]);
                obj->parseExtraData(objEl);

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
    m_lastErrorMessage.clear();
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
    resetEditorLayers();
    clearHistory();
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
    return {cloneObjectList(m_objects), m_objectTags, m_groups, m_nextGroupId,
            m_editorLayers, m_nextLayerId, m_activeLayerId, m_showAllLayers};
}

void ProjectManager::restoreSnapshot(const ProjectSnapshot &snapshot)
{
    m_objects = cloneObjectList(snapshot.objects);
    m_objectTags = snapshot.objectTags;
    m_groups = snapshot.groups;
    m_nextGroupId = snapshot.nextGroupId;
    m_editorLayers = snapshot.layers;
    m_nextLayerId = snapshot.nextLayerId;
    m_activeLayerId = snapshot.activeLayerId;
    m_showAllLayers = snapshot.showAllLayers;
    emit editorLayerObjectsChanged();
    emit editorLayersChanged();
    emit editorLayerViewChanged();
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
    m_clipboardGroups.clear();
}

void ProjectManager::insertObjectAtFront(BaseObject *object, const QString &tagName)
{
    object->setEditorLayerId(m_activeLayerId);
    m_objects.prepend(QSharedPointer<BaseObject>(object));
    m_objectTags.prepend(tagName);
    for (ObjectGroup &group : m_groups) {
        for (int &member : group.members)
            ++member;
    }
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

// метод регистрации стандартых типов объектов (получение словаря поддерживаемых объектов)
void ProjectManager::registerStandardTypes()
{
    // создание экземпляра менеджера объектов
    auto om = ObjectsManager::instance();

    // регистрируем типы объектов
    om->registerType("rectangle", []() { return new RectangleObject(); });
    om->registerType("rectangle_a", []() { return new RectangleObject(); });
    om->registerType("dashline", []() { return new DashedLineObject(); });
    om->registerType("RibonScale", []() { return new RibbonScaleObject(); });
    om->registerType("rotationobject", []() { return new RotationObject(); });
    om->registerType("staticgroup", []() { return new StaticGroupObject(); });
    om->registerType("aviahorizont", []() { return new AviaHorizonObject(); });
    om->registerType("layer3d", []() { return new ThreeDViewObject(); });
    om->registerType("text", []() { return new TextObject(); });
}

int ProjectManager::addObject(const QString &typeName)
{
    auto *registry = FpgaSchemaRegistry::instance();

    if (!m_document->hasCanvas()) {
        emit logMessage(tr("Сначала откройте или создайте проект"));
        return -1;
    }

    if (typeName == QStringLiteral("layer3d")) {
        for (int i = 0; i < m_objects.size(); ++i) {
            if (dynamic_cast<ThreeDViewObject*>(m_objects[i].data())) {
                emit logMessage(tr("3D-вид уже добавлен в сцену"));
                return i;
            }
        }
    }

    BaseObject *rawObject = ObjectsManager::instance()->createObject(typeName);
    if (!rawObject) {
        emit logMessage(tr("Неизвестный тип объекта: %1").arg(typeName));
        return -1;
    }

    const int offset = 24 * (m_objects.size() % 6);
    const double marginX = qMin(40.0, m_document->canvasWidth() * 0.5);
    const double marginY = qMin(40.0, m_document->canvasHeight() * 0.5);
    const double centerX = qBound(marginX, m_document->canvasWidth() * 0.35 + offset,
                                   m_document->canvasWidth() - marginX);
    const double centerY = qBound(marginY, m_document->canvasHeight() * 0.35 + offset,
                                   m_document->canvasHeight() - marginY);

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
    else if (auto threeDView = dynamic_cast<ThreeDViewObject*>(rawObject)) {
        threeDView->setViewVisible(true);
        threeDView->setExportEnabled(true);
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

    const QString schemaName = typeName;
    if (!m_schemas.contains(schemaName)) {
        ParamSchema schema = registry->buildSchema(schemaName);
        if (!schema.isEmpty()) {
            m_schemas.insert(schemaName, schema);
        }
    }

    recordHistory();
    insertObjectAtFront(rawObject, typeName);

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
    if (dynamic_cast<AviaHorizonObject*>(obj.data()) || dynamic_cast<ThreeDViewObject*>(obj.data())) {
        emit projectChanged();
        return index;
    }

    // Перемещаем объект так, чтобы его центр оказался в точке (x, y)
    const QRectF bounds = obj->getBoundingRect();
    const double dx = x - bounds.center().x();
    const double dy = y - bounds.center().y();
    obj->moveBy(dx, dy);
    moveObjectInsideCanvas(obj.data(), m_document->canvasWidth(), m_document->canvasHeight());

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

int ProjectManager::mergeStaticGroups(const QList<int> &indexes, QString *errorMessage)
{
    if (errorMessage) errorMessage->clear();
    auto fail = [errorMessage](const QString &message) {
        if (errorMessage) *errorMessage = message;
        return -1;
    };
    QList<int> selected;
    for (int index : indexes) {
        if (index < 0 || index >= m_objects.size())
            return fail(tr("В выделении есть отсутствующий объект."));
        if (!selected.contains(index)) selected.append(index);
    }
    std::sort(selected.begin(), selected.end());
    if (selected.size() < 2)
        return fail(tr("Выберите минимум два статических изображения или статические группы."));

    auto merged = QSharedPointer<StaticGroupObject>::create();
    merged->setEditorLayerId(m_objects[selected.first()]->editorLayerId());
    const bool exportEnabled = m_objects[selected.first()]->isExportEnabled();
    int nextAddress = 0;
    bool resizeLocked = false;
    // Внутри группы последний init расположен на переднем плане, как при экспорте.
    for (auto it = selected.crbegin(); it != selected.crend(); ++it) {
        const auto source = m_objects[*it];
        if (source->editorLayerId() != merged->editorLayerId())
            return fail(tr("Для объединения в staticgroup переместите объекты на один слой."));
        if (source->isSelectionLocked())
            return fail(tr("Заблокированные объекты нельзя объединять."));
        if (source->isExportEnabled() != exportEnabled)
            return fail(tr("Для объединения у объектов должен совпадать флаг включения в экспорт."));
        if (source->isViewVisible() != m_objects[selected.first()]->isViewVisible())
            return fail(tr("Для объединения у объектов должна совпадать видимость. Включите или выключите глазик у выбранных объектов."));
        resizeLocked |= !source->canResize();
        QList<GroupState> states;
        QList<QImage> masks;
        if (const auto *image = dynamic_cast<const ImageObject *>(source.data())) {
            if (image->isRotationObject())
                return fail(tr("Объединять можно только статические изображения и staticgroup. Вращающиеся объекты и текст не поддерживаются."));
            const auto components = image->maskComponents();
            if (components.isEmpty()) {
                states.append({qRound(image->x), qRound(image->y), qMax(1,qRound(image->width)), qMax(1,qRound(image->height)), 0, image->effectiveMaskColor(), true});
                masks.append(image->renderedImage());
            } else {
                for (const auto &component : components) {
                    states.append({qRound(image->x + component.bounds.x()), qRound(image->y + component.bounds.y()),
                        component.bounds.width(), component.bounds.height(), 0, component.color, true});
                    masks.append(component.mask);
                }
            }
        } else if (const auto *group = dynamic_cast<const StaticGroupObject *>(source.data()); group && !dynamic_cast<const TextObject *>(group)) {
            states = group->states;
            masks = group->maskImages;
        } else {
            return fail(tr("Объединять можно только статические изображения и staticgroup. Вращающиеся объекты и текст не поддерживаются."));
        }
        if (states.isEmpty() || masks.size() != states.size())
            return fail(tr("В статическом объекте отсутствуют растровые маски."));
        if (merged->states.size() + states.size() > FpgaResourceLimits::staticParameters)
            return fail(tr("Объединённой группе требуется %1 init; максимум — 8. Учитываются все цветовые слои изображений.")
                .arg(merged->states.size() + states.size()));
        for (int i = 0; i < states.size(); ++i) {
            auto state = states[i];
            const qint64 area = qint64(state.w) * state.h;
            if (state.w <= 0 || state.h <= 0 || state.w > 4095 || state.h > 4095 || masks[i].isNull())
                return fail(tr("Некорректные габариты или маска статического объекта."));
            if (qint64(nextAddress) + area > FpgaResourceLimits::staticPixels)
                return fail(tr("Растрам объединённой группы требуется более 65536 ячеек. Уменьшите изображения или объедините меньше объектов."));
            state.addr = nextAddress;
            nextAddress += int(area);
            merged->states.append(state);
            merged->maskImages.append(masks[i].size() == QSize(state.w, state.h)
                ? masks[i] : masks[i].scaled(state.w, state.h, Qt::IgnoreAspectRatio, Qt::FastTransformation));
        }
    }
    // Нельзя незаметно поднять нижний слой над пересекающимся промежуточным объектом.
    for (int index = selected.first(); index <= selected.last(); ++index) {
        if (selected.contains(index)) continue;
        for (int member : selected) {
            if (member > index && m_objects[index]->getBoundingRect().intersects(m_objects[member]->getBoundingRect()))
                return fail(tr("Между выбранными слоями есть пересекающийся объект. Сначала расположите объединяемые слои рядом в списке, чтобы сохранить порядок наложения."));
        }
    }
    merged->groupNumber = merged->states.size();
    merged->setCustomName(tr("Статическая группа"));
    merged->setExportEnabled(exportEnabled);
    merged->setViewVisible(m_objects[selected.first()]->isViewVisible());
    merged->setResizeLocked(resizeLocked);
    merged->setImportedHardwareObject(resizeLocked);
    const auto report = FpgaResourceLimits::inspect(*merged);
    if (!report.errors.isEmpty())
        return fail(report.errors.join('\n'));

    recordHistory();
    QList<QSharedPointer<BaseObject>> objects;
    QStringList tags;
    QMap<int,int> oldToNew;
    int mergedIndex = -1;
    for (int i = 0; i < m_objects.size(); ++i) {
        if (i == selected.first()) {
            mergedIndex = objects.size(); objects.append(merged); tags.append("staticgroup");
        }
        if (selected.contains(i)) continue;
        oldToNew.insert(i, objects.size()); objects.append(m_objects[i]); tags.append(m_objectTags.value(i));
    }
    m_objects = objects; m_objectTags = tags;
    for (auto &group : m_groups) {
        QList<int> members;
        for (int old : group.members) if (oldToNew.contains(old)) members.append(oldToNew[old]);
        group.members = members;
    }
    m_groups.erase(std::remove_if(m_groups.begin(),m_groups.end(),[](const ObjectGroup &group){ return group.members.size()<2; }),m_groups.end());
    emit projectChanged();
    emit logMessage(tr("Объединено статических объектов: %1; init: %2/8, память: %3/65536.")
        .arg(selected.size()).arg(merged->states.size()).arg(nextAddress));
    return mergedIndex;
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
    syncNextGroupId();

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

bool ProjectManager::addObjectToGroup(int groupId, int index, bool recordChange)
{
    if (index < 0 || index >= m_objects.size())
        return false;

    for (ObjectGroup &group : m_groups) {
        if (group.id != groupId)
            continue;
        if (group.members.contains(index))
            return true;

        if (recordChange)
            recordHistory();
        group.members.append(index);
        std::sort(group.members.begin(), group.members.end());
        emit projectChanged();
        return true;
    }

    return false;
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

bool ProjectManager::setGroupViewVisible(int groupId, bool visible)
{
    QList<int> members;
    for (const ObjectGroup &group : m_groups) {
        if (group.id == groupId) {
            members = group.members;
            break;
        }
    }
    if (members.isEmpty())
        return false;

    bool hasChanges = false;
    for (int index : members) {
        if (index >= 0 && index < m_objects.size() && m_objects[index]->isViewVisible() != visible) {
            hasChanges = true;
            break;
        }
    }
    if (!hasChanges)
        return true;

    recordHistory();
    for (int index : members) {
        if (index >= 0 && index < m_objects.size())
            m_objects[index]->setViewVisible(visible);
    }
    emit projectChanged();
    return true;
}

bool ProjectManager::setGroupExportEnabled(int groupId, bool enabled)
{
    QList<int> members;
    for (const ObjectGroup &group : m_groups) {
        if (group.id == groupId) {
            members = group.members;
            break;
        }
    }
    if (members.isEmpty())
        return false;

    bool hasChanges = false;
    for (int index : members) {
        if (index >= 0 && index < m_objects.size() && m_objects[index]->isExportEnabled() != enabled) {
            hasChanges = true;
            break;
        }
    }
    if (!hasChanges)
        return true;

    recordHistory();
    for (int index : members) {
        if (index >= 0 && index < m_objects.size())
            m_objects[index]->setExportEnabled(enabled);
    }
    emit projectChanged();
    return true;
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
    m_clipboardGroups.clear();

    QSet<int> seen;
    QMap<int, int> sourceToClipboard;
    for (int index : indexes) {
        if (index < 0 || index >= m_objects.size() || seen.contains(index))
            continue;
        seen.insert(index);
        sourceToClipboard.insert(index, m_clipboardObjects.size());
        m_clipboardObjects.append(cloneObject(m_objects[index]));
        m_clipboardTags.append(index < m_objectTags.size() ? m_objectTags[index] : QString());
    }

    if (m_clipboardObjects.isEmpty())
        return false;

    for (const ObjectGroup &group : m_groups) {
        bool groupFullyCopied = !group.members.isEmpty();
        ObjectGroup copiedGroup;
        copiedGroup.id = group.id;
        copiedGroup.name = group.name;
        for (int member : group.members) {
            if (!sourceToClipboard.contains(member)) {
                groupFullyCopied = false;
                break;
            }
            copiedGroup.members.append(sourceToClipboard.value(member));
        }
        if (groupFullyCopied && copiedGroup.members.size() >= 2) {
            std::sort(copiedGroup.members.begin(), copiedGroup.members.end());
            m_clipboardGroups.append(copiedGroup);
        }
    }

    emit logMessage(tr("Скопировано объектов: %1").arg(m_clipboardObjects.size()));
    return true;
}

void ProjectManager::syncNextGroupId()
{
    int nextId = 1;
    for (const ObjectGroup &group : m_groups)
        nextId = qMax(nextId, group.id + 1);
    m_nextGroupId = qMax(m_nextGroupId, nextId);
}

QList<int> ProjectManager::pasteObjects()
{
    QList<int> pastedIndexes;
    if (m_clipboardObjects.isEmpty())
        return pastedIndexes;

    recordHistory();
    const int pasteCount = m_clipboardObjects.size();
    for (ObjectGroup &group : m_groups) {
        for (int &member : group.members)
            member += pasteCount;
    }

    for (int i = m_clipboardObjects.size() - 1; i >= 0; --i) {
        const auto clone = cloneObject(m_clipboardObjects[i]);
        if (!clone)
            continue;
        clone->setEditorLayerId(m_activeLayerId);
        clone->moveBy(24.0, 24.0);
        moveObjectInsideCanvas(clone.data(), m_document->canvasWidth(), m_document->canvasHeight());
        m_objects.prepend(clone);
        m_objectTags.prepend(i < m_clipboardTags.size() ? m_clipboardTags[i] : QString());
    }

    for (int i = 0; i < m_clipboardObjects.size(); ++i)
        pastedIndexes.append(i);

    syncNextGroupId();
    for (const ObjectGroup &clipboardGroup : m_clipboardGroups) {
        ObjectGroup group;
        group.id = m_nextGroupId++;
        group.name = clipboardGroup.name;
        if (group.name.isEmpty() || group.name.startsWith(tr("Группа ")))
            group.name = tr("Группа %1").arg(group.id);
        group.members = clipboardGroup.members;
        group.members.erase(std::remove_if(group.members.begin(), group.members.end(), [pasteCount](int member) {
            return member < 0 || member >= pasteCount;
        }), group.members.end());
        std::sort(group.members.begin(), group.members.end());
        group.members.erase(std::unique(group.members.begin(), group.members.end()), group.members.end());
        if (group.members.size() >= 2)
            m_groups.append(group);
    }

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

bool ProjectManager::setObjectSelectionLocked(int index, bool locked)
{
    const auto object = getObjectAt(index);
    if (!object || object->isSelectionLocked() == locked)
        return false;
    recordHistory();
    object->setSelectionLocked(locked);
    emit projectChanged();
    return true;
}

bool ProjectManager::setGroupSelectionLocked(int groupId, bool locked)
{
    const auto members = groupMembers(groupId);
    bool changed = false;
    for (int member : members) {
        const auto object = getObjectAt(member);
        if (object && object->isSelectionLocked() != locked) {
            if (!changed) recordHistory();
            object->setSelectionLocked(locked);
            changed = true;
        }
    }
    if (changed) emit projectChanged();
    return changed;
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

bool ProjectManager::setObjectProperty(BaseObject *object, const QString &name, const QString &value)
{
    for (const auto &candidate : m_objects)
        if (candidate.data() == object) return setObjectsProperty({candidate}, name, value);
    return false;
}

bool ProjectManager::isObjectPropertyEditable(const BaseObject *object, const QString &name) const
{
    for (const auto &candidate : m_objects) {
        if (candidate.data() != object) continue;
        const auto probe = cloneObject(candidate);
        if (!probe) return false;
        for (const auto &property : candidate->getProperties())
            if (property.first == name) return probe->setObjectProperty(name, property.second);
    }
    return false;
}

bool ProjectManager::setObjectsProperty(const QList<QSharedPointer<BaseObject>> &objects,
                                       const QString &name, const QString &value)
{
    m_lastErrorMessage.clear();
    QList<QSharedPointer<BaseObject>> changed;
    for (const auto &object : objects) {
        if (!object || !m_objects.contains(object)) return false;
        const auto properties = object->getProperties();
        const auto it = std::find_if(properties.cbegin(), properties.cend(), [&name](const auto &p) { return p.first == name; });
        if (it == properties.cend()) return false;
        if (it->second == value || changed.contains(object)) continue;
        const auto probe = cloneObject(object);
        if (!probe || !probe->setObjectProperty(name, value)) {
            m_lastErrorMessage = probe ? probe->lastValidationMessage() : QString();
            if (m_lastErrorMessage.isEmpty()) m_lastErrorMessage = tr("Значение параметра «%1» недопустимо для одного из выбранных объектов.").arg(name);
            return false;
        }
        const QRectF bounds = probe->getBoundingRect();
        if (!qIsFinite(bounds.x()) || !qIsFinite(bounds.y()) || !qIsFinite(bounds.width()) || !qIsFinite(bounds.height())
            || bounds.width() < 0 || bounds.height() < 0) {
            m_lastErrorMessage = tr("Координаты должны быть конечными числами, а размеры не могут быть отрицательными.");
            return false;
        }
        changed.append(object);
    }
    if (changed.isEmpty()) return !objects.isEmpty();
    recordHistory();
    for (const auto &object : changed) {
        object->setObjectProperty(name, value);
        moveObjectInsideCanvas(object.data(), m_document->canvasWidth(), m_document->canvasHeight());
    }
    emit projectChanged();
    return true;
}

void ProjectManager::resetEditorLayers()
{
    m_editorLayers = {{1, tr("Слой 1"), QColor(QStringLiteral("#00c8f0"))}};
    m_nextLayerId = 2;
    m_activeLayerId = 1;
    m_showAllLayers = false;
}

bool ProjectManager::hasEditorLayer(int id) const
{
    return std::any_of(m_editorLayers.cbegin(), m_editorLayers.cend(), [id](const auto &layer) { return layer.id == id; });
}

QString ProjectManager::editorLayerName(int id) const
{
    for (const auto &layer : m_editorLayers) if (layer.id == id) return layer.name;
    return {};
}

QColor ProjectManager::editorLayerColor(int id) const
{
    for (const auto &layer : m_editorLayers) if (layer.id == id) return layer.selectionColor;
    return QColor(QStringLiteral("#00c8f0"));
}

int ProjectManager::layerObjectCount(int id) const
{
    return std::count_if(m_objects.cbegin(), m_objects.cend(), [id](const auto &o) { return o->editorLayerId() == id; });
}

bool ProjectManager::isObjectOnVisibleLayer(const BaseObject *object) const
{
    return object && (m_showAllLayers || object->editorLayerId() == m_activeLayerId);
}

bool ProjectManager::isObjectVisibleInEditor(const BaseObject *object) const
{
    return isObjectOnVisibleLayer(object) && object->isViewVisible();
}

void ProjectManager::setActiveLayer(int id)
{
    if (!hasEditorLayer(id) || id == m_activeLayerId) return;
    m_activeLayerId = id;
    emit editorLayerViewChanged();
}

void ProjectManager::setShowAllLayers(bool enabled)
{
    if (enabled == m_showAllLayers) return;
    m_showAllLayers = enabled;
    emit editorLayerViewChanged();
}

void ProjectManager::notifyEditorLayersChanged()
{
    emit editorLayersChanged();
    emit editorLayerViewChanged();
    emit projectChanged();
}

QString ProjectManager::uniqueLayerName(const QString &base) const
{
    QString name = base;
    int suffix = 2;
    auto exists = [this](const QString &candidate) {
        return std::any_of(m_editorLayers.cbegin(), m_editorLayers.cend(), [&candidate](const auto &l) { return l.name.compare(candidate, Qt::CaseInsensitive) == 0; });
    };
    while (exists(name)) name = base + QStringLiteral(" (%1)").arg(suffix++);
    return name;
}

int ProjectManager::addEditorLayer()
{
    recordHistory();
    const int id = m_nextLayerId++;
    static const QStringList colors = {"#00c8f0", "#ff5c7a", "#ffca58", "#8bdff2"};
    m_editorLayers.append({id, uniqueLayerName(tr("Слой %1").arg(id)), QColor(colors[(id - 1) % colors.size()])});
    m_activeLayerId = id;
    notifyEditorLayersChanged();
    return id;
}

bool ProjectManager::renameEditorLayer(int id, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || !hasEditorLayer(id)) return false;
    for (const auto &layer : m_editorLayers)
        if (layer.id != id && layer.name.compare(trimmed, Qt::CaseInsensitive) == 0) return false;
    for (int i = 0; i < m_editorLayers.size(); ++i) {
        if (m_editorLayers.at(i).id != id) continue;
        if (m_editorLayers.at(i).name == trimmed) return true;
        recordHistory();
        m_editorLayers[i].name = trimmed;
        notifyEditorLayersChanged();
        return true;
    }
    return false;
}

bool ProjectManager::setEditorLayerColor(int id, const QColor &color)
{
    if (!color.isValid()) return false;
    for (int i = 0; i < m_editorLayers.size(); ++i) {
        if (m_editorLayers.at(i).id != id) continue;
        if (m_editorLayers.at(i).selectionColor == color) return true;
        recordHistory();
        m_editorLayers[i].selectionColor = color;
        notifyEditorLayersChanged();
        return true;
    }
    return false;
}

bool ProjectManager::setObjectsLayer(const QList<QSharedPointer<BaseObject>> &objects, int layerId)
{
    if (!hasEditorLayer(layerId) || objects.isEmpty()) return false;
    for (const auto &object : objects) if (!object || !m_objects.contains(object)) return false;
    if (std::all_of(objects.cbegin(), objects.cend(), [layerId](const auto &o) { return o->editorLayerId() == layerId; })) return true;
    recordHistory();
    for (const auto &object : objects) object->setEditorLayerId(layerId);
    notifyEditorLayersChanged();
    return true;
}

int ProjectManager::duplicateEditorLayer(int id)
{
    m_lastErrorMessage.clear();
    if (!hasEditorLayer(id)) return -1;
    QList<QSharedPointer<BaseObject>> copies;
    QStringList tags;
    QMap<int, int> copiedIndexes;
    for (int i = 0; i < m_objects.size(); ++i) {
        if (m_objects[i]->editorLayerId() != id) continue;
        if (dynamic_cast<ThreeDViewObject *>(m_objects[i].data())) {
            m_lastErrorMessage = tr("Нельзя дублировать слой с 3D-видом: в проекте допустим только один 3D-вид.");
            return -1;
        }
        const auto copy = cloneObject(m_objects[i]);
        if (!copy) return -1;
        copiedIndexes.insert(i, copies.size());
        copies.append(copy);
        tags.append(m_objectTags.value(i));
    }
    recordHistory();
    const int newId = m_nextLayerId++;
    m_editorLayers.append({newId, uniqueLayerName(editorLayerName(id) + tr(" — копия")), editorLayerColor(id)});
    auto groups = m_groups;
    for (auto &group : m_groups) for (int &index : group.members) index += copies.size();
    for (const auto &group : groups) {
        ObjectGroup copy{m_nextGroupId, group.name, {}};
        for (int index : group.members) if (copiedIndexes.contains(index)) copy.members.append(copiedIndexes.value(index));
        if (copy.members.size() >= 2) { ++m_nextGroupId; m_groups.append(copy); }
    }
    for (const auto &copy : copies) copy->setEditorLayerId(newId);
    m_objects = copies + m_objects;
    m_objectTags = tags + m_objectTags;
    m_activeLayerId = newId;
    emit editorLayerObjectsChanged();
    notifyEditorLayersChanged();
    return newId;
}

bool ProjectManager::mergeEditorLayer(int sourceId, int targetId)
{
    return mergeEditorLayers({sourceId, targetId}, targetId);
}

bool ProjectManager::mergeEditorLayers(const QList<int> &layerIds, int targetId)
{
    const QSet<int> selected(layerIds.cbegin(), layerIds.cend());
    if (selected.size() < 2 || !selected.contains(targetId)) return false;
    for (int id : selected) if (!hasEditorLayer(id)) return false;
    recordHistory();
    for (const auto &object : m_objects)
        if (selected.contains(object->editorLayerId())) object->setEditorLayerId(targetId);
    m_editorLayers.erase(std::remove_if(m_editorLayers.begin(), m_editorLayers.end(), [&selected, targetId](const auto &layer) {
        return layer.id != targetId && selected.contains(layer.id);
    }), m_editorLayers.end());
    m_activeLayerId = targetId;
    notifyEditorLayersChanged();
    return true;
}

bool ProjectManager::removeEditorLayer(int id)
{
    if (m_editorLayers.size() <= 1 || !hasEditorLayer(id)) return false;
    recordHistory();
    QMap<int, int> remaining;
    for (int i = m_objects.size() - 1; i >= 0; --i) {
        if (m_objects[i]->editorLayerId() == id) { m_objects.removeAt(i); m_objectTags.removeAt(i); }
    }
    // Сопоставление старых индексов по сохраненным объектам снимка.
    const auto &before = m_undoStack.last().objects;
    int next = 0;
    for (int i = 0; i < before.size(); ++i) if (before[i]->editorLayerId() != id) remaining.insert(i, next++);
    for (auto &group : m_groups) {
        QList<int> members;
        for (int index : group.members) if (remaining.contains(index)) members.append(remaining.value(index));
        group.members = members;
    }
    m_groups.erase(std::remove_if(m_groups.begin(), m_groups.end(), [](const auto &g) { return g.members.size() < 2; }), m_groups.end());
    int removedPosition = 0;
    for (int i = 0; i < m_editorLayers.size(); ++i) if (m_editorLayers[i].id == id) removedPosition = i;
    m_editorLayers.removeAt(removedPosition);
    if (m_activeLayerId == id) m_activeLayerId = m_editorLayers[qMin(removedPosition, int(m_editorLayers.size()) - 1)].id;
    emit editorLayerObjectsChanged();
    notifyEditorLayersChanged();
    return true;
}

bool ProjectManager::saveToFile(const QString &targetFile)
{
    return exportToFpgaXml(targetFile);
}

QDomDocument ProjectManager::buildCompiledFpgaXml() const
{
    QMap<int, FpgaFont> generatedFonts;
    return buildCompiledXmlDocument(
        m_document->projectName(),
        m_document->canvasWidth(),
        m_document->canvasHeight(),
        m_document->backgroundColor(),
        m_objects,
        m_objectTags,
        &generatedFonts
    );
}

bool ProjectManager::exportToFpgaXml(const QString &targetFile)
{
    for (const auto &object : m_objects) {
        if (!object->isExportEnabled()) continue;
        const auto report = FpgaResourceLimits::inspect(*object);
        if (!report.errors.isEmpty()) {
            m_lastErrorMessage = report.errors.join('\n');
            return false;
        }
    }
    const QString outPath = targetFile.isEmpty() ? m_filePath : targetFile;
    if (outPath.isEmpty()) {
        m_lastErrorMessage = tr("Не задан путь для сохранения XML");
        return false;
    }

    const QDomDocument doc = buildCompiledFpgaXml();

    // записываем XML обратно в файл
    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastErrorMessage = tr("Не удалось открыть файл для записи: %1").arg(outFile.errorString());
        return false;
    }

    QString xmlText = QStringLiteral("<?xml version='1.0' encoding='windows-1251'?>\n") + doc.toString(4);
    QByteArray encodedXml = encodeXmlAsWindows1251(xmlText);
    if (outFile.write(encodedXml) != encodedXml.size()) {
        m_lastErrorMessage = tr("Не удалось записать XML: %1").arg(outFile.errorString());
        outFile.close();
        return false;
    }
    outFile.close();

    emit logMessage(tr("Кадр для ПЛИС сохранён: %1").arg(outPath));
    m_lastErrorMessage.clear();
    m_filePath = outPath;
    return true;
}

int ProjectManager::importImage(const QString &fileName, bool rotating)
{
    if (!m_document->hasCanvas()) {
        emit logMessage(tr("Сначала создайте или откройте проект"));
        return -1;
    }

    QImage image;
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    auto *imageObject = new ImageObject(rotating ? ImageObject::Kind::Rotating : ImageObject::Kind::Static);
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

    const ParamSchema staticSchema = m_schemas.value(QStringLiteral("staticgroup"),
        FpgaSchemaRegistry::instance()->buildSchema(QStringLiteral("staticgroup")));
    const QSizeF hardwareFitted = fitSizeToStaticGroupLimits(QSizeF(imageObject->width, imageObject->height), staticSchema);
    imageObject->width = hardwareFitted.width();
    imageObject->height = hardwareFitted.height();

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

// геттеры
int ProjectManager::getObjectCount() const { return m_objects.size(); }
QString ProjectManager::getProjectName() const { return m_document->projectName(); }
int ProjectManager::getCanvasWidth() const { return m_document->canvasWidth(); }
int ProjectManager::getCanvasHeight() const { return m_document->canvasHeight(); }
QColor ProjectManager::getBackgroundColor() const { return m_document->backgroundColor(); }
QString ProjectManager::getFilePath() const { return m_filePath; }
const QList<QSharedPointer<BaseObject>>& ProjectManager::getObjects() const { return m_objects; }
QList<ProjectManager::ObjectGroup> ProjectManager::objectGroups() const { return m_groups; }
bool ProjectManager::hasThreeDViewObject() const
{
    return threeDViewObject() != nullptr;
}

ThreeDViewObject *ProjectManager::threeDViewObject() const
{
    for (const auto &object : m_objects) {
        if (auto threeDView = dynamic_cast<ThreeDViewObject*>(object.data()))
            return threeDView;
    }
    return nullptr;
}

bool ProjectManager::setAircraftState(double x, double y, double altitude, double course, int screenY, bool recordChange)
{
    auto *object = threeDViewObject();
    if (!object) return false;
    const QString previous = object->serializeInit();
    if (!object->setAircraftState(x, y, altitude, course, screenY)) return false;
    const QString next = object->serializeInit();
    if (recordChange) {
        object->parse(previous, FpgaSchemaRegistry::instance()->buildSchema("layer3d"));
        recordHistory();
        object->parse(next, FpgaSchemaRegistry::instance()->buildSchema("layer3d"));
    }
    const QScopedValueRollback<bool> changingState(m_aircraftStateChange, true);
    emit projectChanged();
    return true;
}
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
    auto settings = ApplicationSettings::create();
    m_gridStep = qBound(1, settings.value("gridStep", 10).toInt(), 1000);

    QString colorStr = settings.value("gridColor", "#3778b4c8").toString();
    m_gridColor = QColor(colorStr);
    m_snapCanvasGuideColor = QColor(settings.value("snapCanvasGuideColor", "#d2ff5c7a").toString());
    m_snapGridGuideColor = QColor(settings.value("snapGridGuideColor", "#d256d3ff").toString());
    m_snapObjectGuideColor = QColor(settings.value("snapObjectGuideColor", "#dcffca58").toString());

    emit projectChanged();
}

// сеттеры
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
