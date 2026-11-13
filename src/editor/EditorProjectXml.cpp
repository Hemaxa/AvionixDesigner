// EditorProjectXml - сохранение метаданных сцены и восстановление объектов из XML проекта.

#include "EditorProjectXml.h"

#include "RectangleObject.h"
#include "DashedLineObject.h"
#include "RibbonScaleObject.h"
#include "AviaHorizonObject.h"
#include "ImageObject.h"
#include "RotationObject.h"
#include "StaticGroupObject.h"
#include "TextObject.h"
#include "ThreeDViewObject.h"
#include "ObjectReflection.h"
#include "FpgaSchemaRegistry.h"
#include "BitParser.h"
#include "fpga/config/FpgaPipelineConfig.h"
#include "fpga/model/FpgaCompiledDocument.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QImageReader>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QSvgRenderer>
#include <QtMath>
#include <climits>

namespace EditorProjectXml {
namespace {
constexpr int kMaxResourceBytes = 64 * 1024 * 1024;

// Fields - чтение и запись полей XML с общей проверкой значений и накоплением ошибки.
// Имена полей стабильны и не зависят от языка интерфейса. Числа сохраняются без
// аппаратного округления; длинные строки — текстовыми узлами, чтобы не терять переводы строк.
class Fields
{
public:
    QDomElement node;
    bool reading;
    QString &error;
    Fields(QDomElement element, bool read, QString &message) : node(element), reading(read), error(message) {}
    void fail(const QString &message) { if (error.isEmpty()) error = message; }
    QString attribute(const char *name) {
        if (!node.hasAttribute(name)) fail(QStringLiteral("В <%1> отсутствует атрибут %2.").arg(node.tagName(), name));
        return node.attribute(name);
    }
    void value(const char *name, int &v) {
        if (!reading) { node.setAttribute(name, v); return; }
        bool ok = false; const int n = attribute(name).toInt(&ok);
        if (!ok) fail(QStringLiteral("Некорректное целое число %1 в <%2>.").arg(name, node.tagName()));
        else v = n;
    }
    void value(const char *name, double &v) {
        if (!reading) {
            if (!qIsFinite(v)) { fail(QStringLiteral("Параметр %1 не является конечным числом.").arg(name)); return; }
            node.setAttribute(name, QString::number(v, 'g', 17)); return;
        }
        bool ok = false; const double n = attribute(name).toDouble(&ok);
        if (!ok || !qIsFinite(n)) fail(QStringLiteral("Некорректное число %1 в <%2>.").arg(name, node.tagName()));
        else v = n;
    }
    void value(const char *name, bool &v) {
        if (!reading) { node.setAttribute(name, v ? "true" : "false"); return; }
        const QString s = attribute(name);
        if (s == "true" || s == "1") v = true;
        else if (s == "false" || s == "0") v = false;
        else fail(QStringLiteral("Некорректный флаг %1 в <%2>.").arg(name, node.tagName()));
    }
    void value(const char *name, QColor &v) {
        if (!reading) { node.setAttribute(name, v.isValid() ? v.name(QColor::HexArgb) : QString()); return; }
        const QString s = attribute(name);
        v = QColor(s);
        if (!s.isEmpty() && !v.isValid()) fail(QStringLiteral("Некорректный цвет %1.").arg(name));
    }
    QDomElement child(const char *name) {
        if (!reading) { auto c = node.ownerDocument().createElement(name); node.appendChild(c); return c; }
        auto c = node.firstChildElement(name);
        if (c.isNull() || !c.nextSiblingElement(name).isNull()) fail(QStringLiteral("Ожидается один <%1> в <%2>.").arg(name, node.tagName()));
        return c;
    }
    void text(const char *name, QString &v) {
        auto c = child(name);
        if (reading) {
            v = c.text();
            if (c.hasAttribute("encoding")) {
                if (c.attribute("encoding") != "base64-utf8") { fail("Неизвестная кодировка строки редактора."); return; }
                auto decoded = QByteArray::fromBase64Encoding(v.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
                if (!decoded) { fail("Повреждена строка редактора."); return; }
                QStringDecoder utf8(QStringDecoder::Utf8);
                v = utf8(decoded.decoded);
                if (utf8.hasError()) fail("Повреждён UTF-8 строки редактора.");
            }
        } else {
            for (int i = 0; i < v.size(); ++i) {
                const auto ch = v[i]; const auto code = ch.unicode();
                if ((code < 32 && code != 9 && code != 10 && code != 13) || code == 0xfffe || code == 0xffff
                    || ch.isLowSurrogate() || (ch.isHighSurrogate() && (i + 1 == v.size() || !v[++i].isLowSurrogate()))) {
                    fail(QStringLiteral("Строка %1 содержит символ, недопустимый в XML 1.0.").arg(name)); return;
                }
            }
            // XML нормализует CR/CRLF. Для таких строк сохраняем исходные байты.
            if (v.contains('\r')) {
                c.setAttribute("encoding", "base64-utf8");
                c.appendChild(node.ownerDocument().createTextNode(QString::fromLatin1(v.toUtf8().toBase64())));
            } else c.appendChild(node.ownerDocument().createTextNode(v));
        }
    }
};

QDomElement append(QDomElement parent, const char *name)
{
    auto node = parent.ownerDocument().createElement(name); parent.appendChild(node); return node;
}

QString hash(const QByteArray &data) { return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()); }

void fontFields(Fields &a, FpgaFont &font)
{
    a.value("index", font.index); a.value("size", font.size); a.value("volume", font.volume); a.text("name", font.name);
    a.text("mask-data", font.maskData);
    auto order = a.child("order");
    if (a.reading) {
        font.glyphOrder.clear();
        for (auto n = order.firstChildElement("glyph"); !n.isNull(); n = n.nextSiblingElement("glyph")) {
            Fields g(n, true, a.error); int code = 0; g.value("code", code);
            if (code < 0 || code > 65535 || font.glyphOrder.contains(QChar(code))) a.fail("Некорректный порядок символов шрифта.");
            font.glyphOrder.append(QChar(code));
        }
    } else {
        for (QChar c : font.glyphOrder) append(order, "glyph").setAttribute("code", c.unicode());
    }
    auto glyphs = a.child("glyphs");
    auto glyphFields = [](Fields &g, int &key, FpgaGlyph &glyph) {
        int literal = glyph.literal.unicode();
        g.value("key", key); g.value("literal", literal); g.value("code", glyph.code);
        g.value("enabled", glyph.enabled); g.value("width", glyph.width); g.value("height", glyph.height);
        g.value("advance", glyph.advance); g.value("bearing-x", glyph.bearingX); g.value("bearing-y", glyph.bearingY);
        g.value("ascent", glyph.ascent); g.value("descent", glyph.descent); g.value("offset", glyph.offset); g.value("size", glyph.size);
        g.text("mask-rows", glyph.maskRows);
        if (g.reading) {
            if (key < 0 || key > 65535 || literal < 0 || literal > 65535 || glyph.width < 0 || glyph.height < 0)
                g.fail("Некорректные метрики символа.");
            glyph.literal = QChar(literal);
        }
    };
    if (a.reading) {
        font.glyphs.clear();
        for (auto n = glyphs.firstChildElement("glyph"); !n.isNull(); n = n.nextSiblingElement("glyph")) {
            FpgaGlyph glyph; int key = 0; Fields g(n, true, a.error); glyphFields(g, key, glyph);
            if (font.glyphs.contains(QChar(key))) a.fail("Повторный символ шрифта.");
            const qint64 area = qint64(glyph.width) * glyph.height;
            QString mask = glyph.maskRows; mask.remove('\n'); mask.remove('\r');
            if (area <= 0 || area > 4095 || glyph.offset < 0 || qint64(glyph.offset) + area > font.maskData.size()
                || mask.size() != area || mask != font.maskData.mid(glyph.offset, int(area))) a.fail("Повреждена маска символа шрифта.");
            font.glyphs.insert(QChar(key), glyph);
        }
        for (QChar c : font.glyphOrder) if (!font.glyphs.contains(c)) a.fail("Порядок шрифта ссылается на отсутствующий символ.");
    } else {
        for (auto it = font.glyphs.begin(); it != font.glyphs.end(); ++it) {
            int key = it.key().unicode(); Fields g(append(glyphs, "glyph"), false, a.error); glyphFields(g, key, it.value());
        }
    }
    auto kerning = a.child("kerning");
    if (a.reading) {
        font.kerningPairs.clear();
        for (auto n = kerning.firstChildElement("pair"); !n.isNull(); n = n.nextSiblingElement("pair")) {
            int left = 0, right = 0, delta = 0; Fields k(n, true, a.error);
            k.value("left", left); k.value("right", right); k.value("delta", delta);
            if (left < 0 || left > 65535 || right < 0 || right > 65535) a.fail("Некорректная пара кернинга.");
            const QString key = QString(QChar(left)) + QChar(right);
            if (font.kerningPairs.contains(key)) a.fail("Повторная пара кернинга.");
            font.kerningPairs.insert(key, delta);
        }
    } else {
        for (auto it = font.kerningPairs.cbegin(); it != font.kerningPairs.cend(); ++it) {
            if (it.key().size() != 2) { a.fail("Некорректная пара кернинга."); continue; }
            auto n = append(kerning, "pair"); n.setAttribute("left", it.key()[0].unicode()); n.setAttribute("right", it.key()[1].unicode()); n.setAttribute("delta", it.value());
        }
    }
    if (font.size <= 0 || font.maskData.contains(QRegularExpression("[^0-7]"))) a.fail("Некорректный шрифтовой ресурс редактора.");
}

// Resources - хранение, дедупликация и проверка изображений и атласов отключённых объектов.
class Resources
{
public:
    QDomElement node;
    bool reading;
    QString &error;
    QMap<QString, QByteArray> binaries;
    QMap<QString, QString> mediaTypes;
    QMap<QString, FpgaFont> fonts;
    QMap<QString, QString> idsByHash;
    QMap<QString, QImage> images;
    Resources(QDomElement element, bool read, QString &message) : node(element), reading(read), error(message) {
        if (!reading) return;
        QSet<QString> ids;
        for (auto n = node.firstChildElement(); !n.isNull(); n = n.nextSiblingElement()) {
            const QString id = n.attribute("id");
            if (id.isEmpty() || ids.contains(id)) { fail("Пустой или повторный идентификатор ресурса."); return; }
            ids.insert(id);
            if (n.tagName() == "binary") {
                if (n.attribute("encoding") != "base64" || n.text().size() > kMaxResourceBytes * 2) { fail("Некорректный двоичный ресурс."); return; }
                auto decoded = QByteArray::fromBase64Encoding(n.text().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
                if (!decoded || decoded.decoded.size() > kMaxResourceBytes || hash(decoded.decoded) != n.attribute("sha256")) { fail("Повреждён двоичный ресурс редактора."); return; }
                binaries.insert(id, decoded.decoded); mediaTypes.insert(id, n.attribute("media-type"));
            } else if (n.tagName() == "font") {
                FpgaFont font; Fields a(n, true, error); fontFields(a, font); fonts.insert(id, font);
            } else { fail("Неизвестный ресурс редактора."); return; }
        }
    }
    void fail(const QString &message) { if (error.isEmpty()) error = message; }
    QString binary(const QByteArray &data, const QString &type) {
        if (data.size() > kMaxResourceBytes) { fail("Ресурс редактора превышает 64 МиБ."); return {}; }
        const QString digest = hash(data), key = type + digest;
        if (idsByHash.contains(key)) return idsByHash.value(key);
        const QString id = QStringLiteral("resource-%1").arg(idsByHash.size() + 1);
        idsByHash.insert(key, id);
        auto n = append(node, "binary"); n.setAttribute("id", id); n.setAttribute("encoding", "base64");
        n.setAttribute("media-type", type); n.setAttribute("sha256", digest);
        n.appendChild(node.ownerDocument().createTextNode(QString::fromLatin1(data.toBase64())));
        return id;
    }
    void image(QDomElement n, QImage &image) {
        if (!reading) {
            if (image.isNull()) { n.setAttribute("ref", ""); return; }
            if (qint64(image.width()) * image.height() > 64 * 1024 * 1024) { fail("Маска редактора слишком велика для повторного открытия."); return; }
            QByteArray data; QBuffer buffer(&data); buffer.open(QIODevice::WriteOnly);
            if (!image.save(&buffer, "PNG")) { fail("Не удалось записать маску объекта."); return; }
            n.setAttribute("ref", binary(data, "image/png")); return;
        }
        const QString id = n.attribute("ref");
        if (id.isEmpty()) { image = {}; return; }
        if (!binaries.contains(id) || mediaTypes.value(id) != "image/png") { fail("Отсутствует PNG-маска объекта."); return; }
        if (!images.contains(id)) {
            QBuffer buffer; buffer.setData(binaries.value(id)); buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer, "PNG"); const QSize size = reader.size();
            if (!size.isValid() || qint64(size.width()) * size.height() > 64 * 1024 * 1024) { fail("Некорректный размер PNG-маски."); return; }
            const QImage decoded = reader.read();
            if (decoded.isNull()) { fail("Не удалось прочитать PNG-маску."); return; }
            images.insert(id, decoded);
        }
        image = images.value(id);
    }
    void atlas(QDomElement n, FpgaFont &font) {
        if (reading) {
            const QString id = n.attribute("ref");
            if (!fonts.contains(id)) { fail("Отсутствует шрифтовой атлас объекта."); return; }
            font = fonts.value(id); return;
        }
        auto f = node.ownerDocument().createElement("font"); Fields a(f, false, error); fontFields(a, font);
        QDomDocument canonical; canonical.appendChild(canonical.importNode(f, true));
        const QString key = "font:" + hash(canonical.toByteArray());
        if (!idsByHash.contains(key)) {
            const QString id = QStringLiteral("resource-%1").arg(idsByHash.size() + 1);
            idsByHash.insert(key, id); f.setAttribute("id", id); node.appendChild(f);
        }
        n.setAttribute("ref", idsByHash.value(key));
    }
};

QString objectType(BaseObject *object)
{
    if (dynamic_cast<SymmetryCopyObject *>(object)) return "symmetry-copy";
    if (dynamic_cast<TextObject *>(object)) return "text";
    if (dynamic_cast<RectangleObject *>(object)) return "rectangle";
    if (dynamic_cast<DashedLineObject *>(object)) return "dashline";
    if (dynamic_cast<RibbonScaleObject *>(object)) return "RibonScale";
    if (dynamic_cast<AviaHorizonObject *>(object)) return "aviahorizont";
    if (dynamic_cast<ThreeDViewObject *>(object)) return "layer3d";
    if (dynamic_cast<ImageObject *>(object)) return "image";
    if (dynamic_cast<StaticGroupObject *>(object)) return "staticgroup";
    if (dynamic_cast<RotationObject *>(object)) return "rotationobject";
    return {};
}

QSharedPointer<BaseObject> createObject(const QDomElement &n)
{
    const QString type = n.attribute("type");
    if (type == "rectangle") return QSharedPointer<RectangleObject>::create();
    if (type == "dashline") return QSharedPointer<DashedLineObject>::create();
    if (type == "RibonScale") return QSharedPointer<RibbonScaleObject>::create();
    if (type == "aviahorizont") return QSharedPointer<AviaHorizonObject>::create();
    if (type == "layer3d") return QSharedPointer<ThreeDViewObject>::create();
    if (type == "text") return QSharedPointer<TextObject>::create();
    if (type == "staticgroup") return QSharedPointer<StaticGroupObject>::create();
    if (type == "rotationobject") return QSharedPointer<RotationObject>::create();
    if (type == "image" && (n.attribute("kind") == "static" || n.attribute("kind") == "rotating"))
        return QSharedPointer<ImageObject>::create(n.attribute("kind") == "rotating" ? ImageObject::Kind::Rotating : ImageObject::Kind::Static);
    return {};
}

void states(Fields &a, StaticGroupObject &group, Resources &resources)
{
    a.value("group-number", group.groupNumber);
    auto list = a.child("states");
    auto stateFields = [](Fields &s, GroupState &v) {
        s.value("x", v.x); s.value("y", v.y); s.value("width", v.w); s.value("height", v.h);
        s.value("address", v.addr); s.value("color", v.color); s.value("enabled", v.enabled);
        if (v.w < 0 || v.h < 0 || v.addr < 0) s.fail("Некорректные параметры состояния маски.");
    };
    if (a.reading) {
        group.states.clear();
        for (auto n = list.firstChildElement("state"); !n.isNull(); n = n.nextSiblingElement("state")) {
            GroupState state; Fields s(n, true, a.error); stateFields(s, state); group.states.append(state);
        }
    } else {
        for (auto &state : group.states) { Fields s(append(list, "state"), false, a.error); stateFields(s, state); }
    }
    auto masks = a.child("masks");
    if (a.reading) {
        group.maskImages.clear();
        for (auto n = masks.firstChildElement("mask"); !n.isNull(); n = n.nextSiblingElement("mask")) {
            QImage image; resources.image(n, image); group.maskImages.append(image);
        }
    } else {
        for (auto &image : group.maskImages) resources.image(append(masks, "mask"), image);
    }
}

void rotationLayer(Fields &a, RotationObjectLayer &v)
{
    a.value("left", v.left); a.value("top", v.top); a.value("right", v.right); a.value("bottom", v.bottom);
    a.value("center-x", v.xRot); a.value("center-y", v.yRot); a.value("sin", v.sinVal); a.value("cos", v.cosVal);
    a.value("color", v.color); a.value("enabled", v.enabled);
}

void objectFields(Fields &a, BaseObject &object, Resources &resources)
{
    if (auto *v = dynamic_cast<RectangleObject *>(&object)) {
        a.value("enabled", v->enabled); a.value("x", v->x); a.value("y", v->y); a.value("width", v->width); a.value("height", v->height);
        a.value("fill", v->fillColor); a.value("stroke", v->strokeColor); a.value("stroke-width", v->strokeWidth);
        a.value("alpha", v->alpha); a.value("explicit-alpha", v->explicitAlpha);
        if (v->alpha < 0 || v->alpha > 255) a.fail("Прозрачность должна быть 0…255.");
    } else if (auto *v = dynamic_cast<DashedLineObject *>(&object)) {
        a.value("enabled", v->enabled); a.value("color", v->color); a.value("x0", v->x0); a.value("y0", v->y0); a.value("x1", v->x1); a.value("y1", v->y1);
        a.value("period", v->dashPeriod); a.value("dash-length", v->dashLength); a.value("phase", v->dashPhase); a.value("line-width", v->lineWidth);
    } else if (auto *v = dynamic_cast<RibbonScaleObject *>(&object)) {
        a.value("enabled", v->enabled); a.value("color", v->color); a.value("left", v->left); a.value("right", v->right);
        a.value("top", v->top); a.value("bottom", v->bottom); a.value("line-width", v->lineWidth); a.value("period", v->period); a.value("start-y", v->yStart);
    } else if (auto *v = dynamic_cast<AviaHorizonObject *>(&object)) {
        a.value("enabled", v->enabled); a.value("earth-color", v->earthColor); a.value("sky-color", v->skyColor); a.value("line-color", v->horizonLineColor);
        a.value("line-width", v->lineWidth); a.value("center-x", v->xCenter); a.value("center-y", v->yCenter);
        a.value("width", v->areaWidth); a.value("height", v->areaHeight); a.value("sin", v->sinVal); a.value("cos", v->cosVal);
    } else if (auto *v = dynamic_cast<ThreeDViewObject *>(&object)) {
        QString init = v->serializeInit(); bool left = v->isLeftAnchored(); a.text("init", init); a.value("left-anchored", left);
        if (a.reading) {
            if (!QRegularExpression("^[0-3][0-9A-Fa-f]{37}$").match(init).hasMatch()) a.fail("Некорректный init 3D-вида.");
            else v->parse(init, FpgaSchemaRegistry::instance()->buildSchema("layer3d"));
            v->setLeftAnchored(left);
        }
    } else if (auto *v = dynamic_cast<ImageObject *>(&object)) {
        if (a.reading) v->sourceName.clear();
        a.text("format", v->format);
        if (v->format != "svg" && v->format != "raster") a.fail("Неизвестный формат исходного изображения.");
        const bool storedLayers = a.reading ? !a.node.firstChildElement("raster-layers").isNull() : !v->restoredRasterComponents().isEmpty();
        if (storedLayers) {
            Fields layers(a.child("raster-layers"), a.reading, a.error);
            auto components = v->restoredRasterComponents();
            QSize size = v->restoredRasterSize();
            int width = size.width(), height = size.height();
            layers.value("width", width); layers.value("height", height);
            if (v->isRotationObject()) {
                auto coefficients = v->rotationCoefficients();
                int sine = coefficients.x(), cosine = coefficients.y();
                layers.value("sin", sine); layers.value("cos", cosine);
                if (a.reading) v->restoreRotationCoefficients(sine, cosine);
            }
            if (width <= 0 || height <= 0 || qint64(width) * height > 64 * 1024 * 1024) { a.fail("Некорректные габариты растровых слоёв."); return; }
            if (a.reading) components.clear();
            auto n = a.reading ? layers.node.firstChildElement("layer") : QDomElement();
            for (int i = 0; a.reading ? !n.isNull() : i < components.size(); ++i) {
                if (!a.reading) n = append(layers.node, "layer");
                ImageMaskComponent c = a.reading ? ImageMaskComponent{} : components[i];
                Fields f(n, a.reading, a.error);
                int x = c.bounds.x(), y = c.bounds.y();
                f.value("x", x); f.value("y", y); f.value("color", c.color);
                resources.image(f.child("mask"), c.mask);
                c.bounds = QRect(QPoint(x, y), c.mask.size()); c.layerIndex = i;
                if (c.mask.isNull() || !QRect(0, 0, width, height).contains(c.bounds)) { a.fail("Растровый слой выходит за границы изображения."); return; }
                if (a.reading) { components.append(c); n = n.nextSiblingElement("layer"); }
            }
            if (a.reading) {
                const auto coefficients = v->rotationCoefficients();
                v->restoreRasterComponents(components, QSize(width, height));
                if (v->isRotationObject()) v->restoreRotationCoefficients(coefficients.x(), coefficients.y());
            }
        } else {
            auto source = a.child("source");
            if (!a.reading) {
                const QByteArray data = v->sourcePayload();
                source.setAttribute("ref", data.isEmpty() ? QString() : resources.binary(data, v->format == "svg" ? "image/svg+xml" : "image/png"));
            } else {
                const QString id = source.attribute("ref");
                if (!id.isEmpty()) {
                    if (!resources.binaries.contains(id) || resources.mediaTypes.value(id) != (v->format == "svg" ? "image/svg+xml" : "image/png")) a.fail("Отсутствует исходное изображение.");
                    else {
                        const QByteArray data = resources.binaries.value(id);
                        if (v->format == "svg") {
                            QSvgRenderer renderer(data);
                            if (!renderer.isValid()) a.fail("Повреждён исходный SVG.");
                        } else {
                            QImage image; resources.image(source, image);
                        }
                        if (a.error.isEmpty()) v->setSourcePayload(data);
                    }
                }
            }
        }
        // После установки исходника: setSourcePayload может установить исходный размер.
        a.value("x", v->x); a.value("y", v->y); a.value("width", v->width); a.value("height", v->height);
        a.value("rotation", v->rotationDegrees); a.value("mask-color", v->maskColor); a.value("auto-mask-color", v->autoMaskColor);
        if (!a.reading || a.node.hasAttribute("mirror-x")) a.value("mirror-x", v->mirrorHorizontal);
        if (!a.reading || a.node.hasAttribute("mirror-y")) a.value("mirror-y", v->mirrorVertical);
        auto list = a.child("color-layers");
        auto colorFields = [](Fields &f, ImageColorLayer &c) {
            f.value("source-color", c.sourceColor); f.value("mask-color", c.maskColor); f.value("auto-mask-color", c.autoMaskColor);
            if (!f.reading || f.node.hasAttribute("order")) f.value("order", c.order);
            if ((!f.reading && !c.sourceBounds.isEmpty()) || f.node.hasAttribute("source-x")) {
                double x = c.sourceBounds.x(), y = c.sourceBounds.y(), w = c.sourceBounds.width(), h = c.sourceBounds.height();
                f.value("source-x", x); f.value("source-y", y); f.value("source-width", w); f.value("source-height", h);
                c.sourceBounds = QRectF(x, y, w, h);
                if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > 1.000000001 || y + h > 1.000000001)
                    f.fail("Некорректные границы исходной области изображения.");
            }
        };
        if (a.reading) {
            QList<ImageColorLayer> colors;
            for (auto n = list.firstChildElement("layer"); !n.isNull(); n = n.nextSiblingElement("layer")) {
                ImageColorLayer color; color.order = colors.size();
                Fields f(n, true, a.error); colorFields(f, color); colors.append(color);
            }
            QSet<int> positions;
            for (const auto &color : colors) {
                if (color.order < 0 || color.order >= colors.size() || positions.contains(color.order))
                    a.fail("Некорректный или повторный порядок блоков изображения.");
                positions.insert(color.order);
            }
            v->setColorLayers(colors);
        } else {
            auto colors = v->colorLayers();
            for (auto &c : colors) { Fields f(append(list, "layer"), false, a.error); colorFields(f, c); }
        }
    } else if (auto *v = dynamic_cast<StaticGroupObject *>(&object)) {
        if (auto *text = dynamic_cast<TextObject *>(v)) {
            bool hasAtlas = text->hasFontAtlas(); a.value("has-font-atlas", hasAtlas);
            if (hasAtlas) {
                auto n = a.child("font-atlas"); FpgaFont font = text->fontAtlas(); resources.atlas(n, font);
                if (a.reading && a.error.isEmpty()) text->setFontAtlas(font, true);
            }
            a.text("text", text->text); a.text("font-family", text->fontFamily); a.text("extra-characters", text->extraCharacters);
            a.value("pixel-size", text->pixelSize); a.value("font-index", text->fontIndex); a.value("restricted-atlas-editing", text->restrictedAtlasEditing);
            a.value("symbol-capacity", text->symbolCapacity);
            if (text->symbolCapacity < 0 || text->symbolCapacity > FpgaPipelineConfig::parameterArraySize)
                a.fail("Некорректное число позиций текста для ПЛИС.");
            auto alphabets = a.child("export-alphabets");
            if (a.reading) {
                text->exportAlphabetGroups.clear();
                for (auto n = alphabets.firstChildElement("alphabet"); !n.isNull(); n = n.nextSiblingElement("alphabet")) text->exportAlphabetGroups.append(n.text());
            } else {
                for (const auto &name : text->exportAlphabetGroups) append(alphabets, "alphabet").appendChild(a.node.ownerDocument().createTextNode(name));
            }
            if (text->pixelSize <= 0 || text->pixelSize > 4096) a.fail("Некорректный размер шрифта.");
        }
        states(a, *v, resources);
    } else if (auto *v = dynamic_cast<RotationObject *>(&object)) {
        a.value("enabled", v->enabled); a.value("left", v->left); a.value("top", v->top); a.value("right", v->right); a.value("bottom", v->bottom);
        a.value("center-x", v->xRot); a.value("center-y", v->yRot); a.value("sin", v->sinVal); a.value("cos", v->cosVal); a.value("color", v->color);
        resources.image(a.child("mask"), v->maskImage);
        auto list = a.child("hardware-layers");
        if (a.reading) {
            QVector<RotationObjectLayer> layers;
            for (auto n = list.firstChildElement("layer"); !n.isNull(); n = n.nextSiblingElement("layer")) {
                RotationObjectLayer layer; Fields f(n, true, a.error); rotationLayer(f, layer); layers.append(layer);
            }
            v->restoreHardwareLayers(layers);
        } else {
            auto layers = v->hardwareLayers();
            for (auto &layer : layers) { Fields f(append(list, "layer"), false, a.error); rotationLayer(f, layer); }
        }
    }
}

QImage maskImage(const QString &data, int offset, const QSize &size, const QColor &color)
{
    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    for (int y = 0; y < size.height(); ++y)
        for (int x = 0; x < size.width(); ++x) {
            QColor pixel = color;
            // Обратное преобразование сериализатора растров; уровни сохраняются точно.
            pixel.setAlpha(data[offset + y * size.width() + x].digitValue() * 255 / 7);
            image.setPixelColor(x, y, pixel);
        }
    return image;
}

bool restoreHardware(BaseObject &object, const QDomElement &element,
                     const FpgaCompiledDocument &document, bool restrictedText, QString &error)
{
    const QString tag = element.tagName();
    const auto schema = document.schemas.value(tag);
    const QString init = element.firstChildElement("init").text().trimmed();
    auto field = [&](const char *name) { const auto f = schema.value(name); return int(BitParser::extract(init, f.offset, f.size)); };
    QString data = element.firstChildElement("data").text();
    data.remove(QRegularExpression("[,\\s]"));
    if (auto *text = dynamic_cast<TextObject *>(&object)) {
        const auto codes = element.firstChildElement("data").text().split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
        QString characters;
        for (const auto &code : codes) characters.append(QChar(code.toUShort()));
        const auto font = document.fonts.constFind(field("font_index"));
        if (font == document.fonts.cend()) { error = "Отсутствует шрифт текста."; return false; }
        text->text = characters.mid(field("char_offset"), field("char_count"));
        text->states = {{field("x"), field("y"), 0, 0, 0, BitParser::parseColor(field("color")), bool(field("enb"))}};
        text->setFontAtlas(font.value(), restrictedText);
        return true;
    }
    if (tag == "staticgroup") {
        StaticGroupObject group;
        group.parse(init, schema);
        // Проверяем все адреса до выделения изображений в parseExtraData().
        for (auto n = element.firstChildElement("init"); !n.isNull(); n = n.nextSiblingElement("init")) {
            auto get = [&](const char *key) { const auto f = schema.value(key); return qint64(BitParser::extract(n.text(), f.offset, f.size)); };
            if (get("w") <= 0 || get("h") <= 0 || get("addr") + get("w") * get("h") > data.size()) {
                error = "Маска STATICGROUP выходит за границы data."; return false;
            }
        }
        if (data.contains(QRegularExpression("[^0-7]"))) { error = "Повреждены уровни маски STATICGROUP."; return false; }
        group.parseExtraData(element);
        if (auto *image = dynamic_cast<ImageObject *>(&object)) {
            QRect bounds;
            for (const auto &s : group.states) bounds = bounds.united(QRect(s.x, s.y, s.w, s.h));
            QList<ImageMaskComponent> components;
            for (int i = 0; i < group.states.size(); ++i) {
                const auto &s = group.states[i];
                components.append({i, QRect(QPoint(s.x, s.y) - bounds.topLeft(), QSize(s.w, s.h)), s.color, group.maskImages[i]});
            }
            image->restoreRasterComponents(components, bounds.size());
            image->x = bounds.x(); image->y = bounds.y();
        } else {
            auto &target = static_cast<StaticGroupObject &>(object);
            target.states = group.states; target.maskImages = group.maskImages; target.groupNumber = group.groupNumber;
        }
        return true;
    }
    if (tag == "rotationobject") {
        RotationObject rotation;
        rotation.parse(init, schema);
        QStringList params;
        for (auto n = element.firstChildElement("init"); !n.isNull(); n = n.nextSiblingElement("init")) params.append(n.text().trimmed());
        rotation.setHardwareLayersFromInitHex(params, schema);
        const auto payload = element.firstChildElement("data");
        const QSize size(payload.attribute("width").toInt(), payload.attribute("height").toInt());
        if (size.width() <= 0 || size.height() <= 0 || qint64(size.width()) * size.height() != data.size()
            || data.contains(QRegularExpression("[^0-7]"))) { error = "Повреждена вращаемая маска."; return false; }
        rotation.maskImage = maskImage(data, 0, size, rotation.color);
        if (auto *image = dynamic_cast<ImageObject *>(&object)) {
            // У обрезанного растра центр вращения может находиться вне маски.
            // Восстанавливаем прозрачные поля так, чтобы сохранить аппаратный центр.
            const int halfWidth = qMax(qAbs(int(rotation.left)), qAbs(int(rotation.right)));
            const int halfHeight = qMax(qAbs(int(rotation.top)), qAbs(int(rotation.bottom)));
            image->restoreRasterComponents({{0, QRect(QPoint(halfWidth + rotation.left, halfHeight + rotation.top), size), rotation.color, rotation.maskImage}},
                QSize(qMax(1, halfWidth * 2), qMax(1, halfHeight * 2)));
            image->x = rotation.xRot - halfWidth; image->y = rotation.yRot - halfHeight;
            image->restoreRotationCoefficients(rotation.sinVal, rotation.cosVal);
        } else {
            auto &target = static_cast<RotationObject &>(object);
            target.parse(init, schema); target.setHardwareLayersFromInitHex(params, schema); target.maskImage = rotation.maskImage;
        }
        return true;
    }
    object.parse(init, schema);
    return true;
}

bool process(QDomElement editor, State &state, bool reading, QString &error)
{
    Fields root(editor, reading, error);
    QList<QDomElement> hardware;
    const auto objectsNode = editor.parentNode().firstChildElement("objects");
    for (auto n = objectsNode.firstChildElement(); !n.isNull(); n = n.nextSiblingElement())
        if (n.tagName() != "font") hardware.append(n);
    FpgaCompiledDocument compiled;
    if (reading) {
        compiled = FpgaCompiledDocument::fromDomDocument(editor.ownerDocument(), &error);
        if (!error.isEmpty()) return false;
    }
    QMap<BaseObject *, int> hardwareIndex;
    if (!reading) {
        int next = 0;
        for (const auto &ptr : state.objects)
            if (dynamic_cast<ThreeDViewObject *>(ptr.data()) && ptr->isExportEnabled()) hardwareIndex.insert(ptr.data(), next++);
        for (int i = state.objects.size() - 1; i >= 0; --i)
            if (state.objects[i]->isExportEnabled() && !dynamic_cast<ThreeDViewObject *>(state.objects[i].data())) hardwareIndex.insert(state.objects[i].data(), next++);
        if (next != hardware.size()) { error = "Не удалось сопоставить объекты редактора с аппаратным XML."; return false; }
    }
    QSet<int> referencedHardware;
    auto workspace = root.child("workspace"); Fields w(workspace, reading, error);
    QString projectName = state.document.projectName(); QColor background = state.document.backgroundColor();
    int width = state.document.canvasWidth(), height = state.document.canvasHeight();
    w.text("project-name", projectName); w.value("canvas-width", width); w.value("canvas-height", height); w.value("background", background);
    w.value("show-grid", state.showGrid); w.value("snap-to-canvas", state.snapToCanvas); w.value("snap-to-grid", state.snapToGrid); w.value("snap-to-objects", state.snapToObjects);
    w.value("grid-step", state.gridStep); w.value("grid-color", state.gridColor); w.value("canvas-guide-color", state.canvasGuideColor);
    w.value("grid-guide-color", state.gridGuideColor); w.value("object-guide-color", state.objectGuideColor);
    Fields view(w.child("view"), reading, error);
    view.value("scale", state.view.scale); double x = state.view.offset.x(), y = state.view.offset.y();
    view.value("offset-x", x); view.value("offset-y", y); view.value("proportional-resize", state.view.proportionalResize); view.value("rotation-snapping", state.view.rotationSnapping);
    if (width <= 0 || height <= 0 || state.gridStep < 1 || state.gridStep > 1000 || state.view.scale < .1 || state.view.scale > 10
        || !background.isValid() || !state.gridColor.isValid() || !state.canvasGuideColor.isValid() || !state.gridGuideColor.isValid() || !state.objectGuideColor.isValid())
        root.fail("Некорректные настройки рабочей области редактора.");
    if (reading) { state.document.setProjectName(projectName); state.document.setCanvasSize(width, height); state.document.setBackgroundColor(background); state.view.offset = QPointF(x, y); }

    Fields layers(root.child("layers"), reading, error);
    layers.value("active-id", state.activeLayerId); layers.value("show-all", state.showAllLayers); layers.value("next-id", state.nextLayerId);
    auto layerFields = [](Fields &a, ProjectManager::EditorLayer &layer) { a.value("id", layer.id); a.text("name", layer.name); a.value("selection-color", layer.selectionColor); };
    if (reading) {
        state.layers.clear();
        for (auto n = layers.node.firstChildElement("layer"); !n.isNull(); n = n.nextSiblingElement("layer")) {
            ProjectManager::EditorLayer layer; Fields a(n, true, error); layerFields(a, layer); state.layers.append(layer);
        }
    } else {
        for (auto &layer : state.layers) { Fields a(append(layers.node, "layer"), false, error); layerFields(a, layer); }
    }
    QSet<int> layerIds; QSet<QString> layerNames; int maxLayer = 0;
    for (const auto &layer : state.layers) {
        if (layer.id <= 0 || layer.id == INT_MAX || layerIds.contains(layer.id) || layer.name.trimmed().isEmpty()
            || layerNames.contains(layer.name.trimmed().toCaseFolded()) || !layer.selectionColor.isValid()) root.fail("Некорректный или повторный слой редактора.");
        layerIds.insert(layer.id); layerNames.insert(layer.name.trimmed().toCaseFolded()); maxLayer = qMax(maxLayer, layer.id);
    }
    if (!layerIds.contains(state.activeLayerId) || state.nextLayerId <= maxLayer || state.nextLayerId == INT_MAX) root.fail("Некорректный активный слой или счётчик слоёв.");

    auto resourcesNode = root.child("resources"); Resources resources(resourcesNode, reading, error);
    QMap<int, QSharedPointer<SymmetryAxis>> axes;
    auto symmetry = editor.firstChildElement("symmetry");
    if (reading) {
        if (!symmetry.nextSiblingElement("symmetry").isNull()) root.fail("Повторный раздел симметрии.");
        for (auto n = symmetry.firstChildElement("axis"); !n.isNull(); n = n.nextSiblingElement("axis")) {
            auto axis = QSharedPointer<SymmetryAxis>::create(); Fields a(n, true, error);
            a.value("id", axis->id); a.value("position", axis->position);
            const auto direction = a.attribute("orientation");
            if (direction != "vertical" && direction != "horizontal") a.fail("Неизвестное направление оси симметрии.");
            axis->orientation = direction == "vertical" ? Qt::Vertical : Qt::Horizontal;
            if (axis->id <= 0 || axis->id == INT_MAX || axes.contains(axis->id) || axis->position < 0
                || axis->position > (axis->orientation == Qt::Vertical ? width : height)) a.fail("Некорректная ось симметрии.");
            axes.insert(axis->id, axis);
        }
    } else {
        for (const auto &ptr : state.objects) {
            const auto *copy = dynamic_cast<SymmetryCopyObject *>(ptr.data());
            if (!copy) continue;
            if (!copy->source() || !state.objects.contains(copy->source()) || !supportsObjectReflection(copy->source().data())) {
                root.fail("У копии отсутствует допустимый исходный объект."); return false;
            }
            if (copy->source()->editorLayerId() != copy->editorLayerId() || copy->axis()->id <= 0 || copy->axis()->id == INT_MAX
                || !qIsFinite(copy->axis()->position) || copy->axis()->position < 0
                || copy->axis()->position > (copy->axis()->orientation == Qt::Vertical ? width : height)) {
                root.fail("Некорректная ось или слой симметричной копии."); return false;
            }
            if (axes.contains(copy->axis()->id) && axes.value(copy->axis()->id) != copy->axis()) {
                root.fail("Повторный идентификатор оси симметрии."); return false;
            }
            axes.insert(copy->axis()->id, copy->axis());
        }
        if (!axes.isEmpty()) {
            symmetry = append(editor, "symmetry");
            for (const auto &axis : axes) {
                Fields a(append(symmetry, "axis"), false, error); a.value("id", axis->id); a.value("position", axis->position);
                a.node.setAttribute("orientation", axis->orientation == Qt::Vertical ? "vertical" : "horizontal");
            }
        }
    }
    auto scene = root.child("scene");
    if (reading && scene.attribute("order") != "front-to-back") root.fail("Неизвестный порядок объектов редактора.");
    if (!reading) scene.setAttribute("order", "front-to-back");
    QMap<QString, int> objectIds;
    QMap<int, QString> copySources;
    QSet<int> usedAxes;
    if (reading) { state.objects.clear(); state.objectTags.clear(); }
    auto object = reading ? scene.firstChildElement("object") : QDomElement();
    const int writeCount = state.objects.size(); int threeDCount = 0;
    for (int index = 0; reading ? !object.isNull() : index < writeCount; ++index) {
        if (!reading) {
            object = append(scene, "object"); object.setAttribute("id", QStringLiteral("object-%1").arg(index + 1));
            object.setAttribute("type", objectType(state.objects[index].data()));
            QString exportTag = state.objectTags.value(index);
            if (const auto *rectangle = dynamic_cast<RectangleObject *>(state.objects[index].data())) exportTag = rectangle->usesAlpha() ? "rectangle_a" : "rectangle";
            object.setAttribute("export-tag", exportTag);
            if (auto *image = dynamic_cast<ImageObject *>(state.objects[index].data())) object.setAttribute("kind", image->isRotationObject() ? "rotating" : "static");
        }
        const QString id = object.attribute("id"), type = object.attribute("type"), tag = object.attribute("export-tag");
        if (id.isEmpty() || objectIds.contains(id)) root.fail("Пустой или повторный идентификатор объекта.");
        objectIds.insert(id, index);
        QSharedPointer<BaseObject> ptr = reading ? createObject(object) : state.objects[index];
        if (reading && type == "symmetry-copy") {
            int axisId = 0; Fields relation(object.firstChildElement("symmetry-copy"), true, error);
            relation.value("axis-id", axisId);
            if (!axes.contains(axisId)) { root.fail("Копия ссылается на отсутствующую ось."); return false; }
            copySources.insert(index, relation.attribute("source")); usedAxes.insert(axisId);
            ptr = QSharedPointer<SymmetryCopyObject>::create(QSharedPointer<BaseObject>{}, axes.value(axisId));
        }
        if (!ptr || type.isEmpty() || (tag != type && !(type == "rectangle" && tag == "rectangle_a"))) { root.fail("Неизвестный тип объекта редактора или неверный export-tag."); return false; }
        if (type == "layer3d" && ++threeDCount > 1) root.fail("В редакторе допускается один 3D-вид.");
        Fields meta(object, reading, error);
        int layer = ptr->editorLayerId(); QString name = ptr->customName(), hint = ptr->editRestrictionHint();
        bool visible = ptr->isViewVisible(), locked = ptr->isSelectionLocked(), exported = ptr->isExportEnabled();
        bool resizeLocked = ptr->isResizeLocked(), imported = ptr->isImportedHardwareObject();
        meta.value("layer-id", layer); meta.text("name", name);
        Fields flags(meta.child("flags"), reading, error);
        flags.value("visible", visible); flags.value("selection-locked", locked); flags.value("export-enabled", exported);
        flags.value("resize-locked", resizeLocked); flags.value("imported-hardware", imported); flags.text("restriction-hint", hint);
        if (type == "symmetry-copy") {
            if (exported) { root.fail("Зависимую копию нельзя включать в аппаратный экспорт."); return false; }
            if (!reading) {
                const auto *copy = static_cast<SymmetryCopyObject *>(ptr.data());
                auto relation = meta.child("symmetry-copy");
                relation.setAttribute("axis-id", copy->axis()->id);
                relation.setAttribute("source", QStringLiteral("object-%1").arg(state.objects.indexOf(copy->source()) + 1));
            }
        } else if (exported) {
            int reference = reading ? -1 : hardwareIndex.value(ptr.data(), -1);
            meta.value("hardware-index", reference);
            const QString expected = type == "image" ? (object.attribute("kind") == "rotating" ? "rotationobject" : "staticgroup") : tag;
            if (reference < 0 || reference >= hardware.size() || referencedHardware.contains(reference)
                || hardware.value(reference).tagName() != expected) { root.fail("Некорректная ссылка на аппаратный объект."); return false; }
            referencedHardware.insert(reference);
            bool restricted = false;
            QDomElement options;
            if (auto *text = dynamic_cast<TextObject *>(ptr.data())) {
                Fields editing(meta.child("text-options"), reading, error);
                options = editing.node;
                restricted = text->restrictedAtlasEditing;
                editing.value("restricted-atlas-editing", restricted);
            }
            const bool scaleParameters = type == "RibonScale" && (!reading || !object.firstChildElement("parameters").isNull());
            if (reading && type == "RibonScale" && !scaleParameters
                && !hardware[reference].firstChildElement("init").nextSiblingElement("init").isNull()) {
                root.fail("У трёхсловной заглушки шкалы отсутствуют исходные параметры редактора."); return false;
            }
            if (reading && !scaleParameters && !restoreHardware(*ptr, hardware[reference], compiled, restricted, error)) return false;
            if (scaleParameters) {
                // Аппаратные init шкалы временно фиксированы. Сохраняем исходные
                // координаты и оформление независимо от заглушки, без округления.
                // Старые проекты без parameters восстанавливаются из прежнего init.
                Fields parameters(meta.child("parameters"), reading, error);
                objectFields(parameters, *ptr, resources);
            }
            if (auto *text = dynamic_cast<TextObject *>(ptr.data())) {
                Fields editing(options, reading, error);
                editing.text("extra-characters", text->extraCharacters);
                editing.value("symbol-capacity", text->symbolCapacity);
                if (text->symbolCapacity < 0 || text->symbolCapacity > FpgaPipelineConfig::parameterArraySize)
                    editing.fail("Некорректное число позиций текста для ПЛИС.");
                auto alphabets = editing.child("export-alphabets");
                if (reading) {
                    text->exportAlphabetGroups.clear();
                    for (auto n = alphabets.firstChildElement("alphabet"); !n.isNull(); n = n.nextSiblingElement("alphabet")) text->exportAlphabetGroups.append(n.text());
                } else for (const auto &name : text->exportAlphabetGroups) append(alphabets, "alphabet").appendChild(editor.ownerDocument().createTextNode(name));
            }
            if (auto *view = dynamic_cast<ThreeDViewObject *>(ptr.data())) {
                bool left = view->isLeftAnchored(); meta.value("left-anchored", left); if (reading) view->setLeftAnchored(left);
            }
        } else {
            Fields parameters(meta.child("parameters"), reading, error); objectFields(parameters, *ptr, resources);
        }
        auto pivotNode = object.firstChildElement("rotation-pivot");
        if ((!reading && ptr->editorRotationPivot()) || (reading && !pivotNode.isNull())) {
            const auto *image = dynamic_cast<ImageObject *>(ptr.data());
            if ((!image || !image->isRotationObject()) && !dynamic_cast<RotationObject *>(ptr.data())) {
                root.fail("Редакторский центр вращения задан у неподдерживаемого объекта."); return false;
            }
            if (!reading) pivotNode = append(object, "rotation-pivot");
            if (reading && !pivotNode.nextSiblingElement("rotation-pivot").isNull()) {
                root.fail("Повторный центр вращения объекта."); return false;
            }
            QPointF point = reading ? QPointF() : ptr->rotationCenter();
            double x = point.x(), y = point.y(); Fields pivot(pivotNode, reading, error);
            pivot.value("canvas-x", x); pivot.value("canvas-y", y);
            if (reading) {
                bool invertible = false;
                const auto inverse = ptr->rotationFrame().inverted(&invertible);
                const auto local = inverse.map(QPointF(x, y));
                if (!invertible || !qIsFinite(local.x()) || !qIsFinite(local.y())) {
                    root.fail("Некорректный центр вращения объекта."); return false;
                }
                ptr->setEditorRotationPivot(local);
            }
        }
        if (!layerIds.contains(layer)) root.fail("Объект ссылается на отсутствующий слой.");
        if (reading) {
            ptr->setEditorLayerId(layer); ptr->setCustomName(name); ptr->setViewVisible(visible); ptr->setSelectionLocked(locked);
            ptr->setExportEnabled(exported); ptr->setResizeLocked(resizeLocked); ptr->setImportedHardwareObject(imported); ptr->setEditRestrictionHint(hint);
            state.objects.append(ptr); state.objectTags.append(tag);
            object = object.nextSiblingElement("object");
        }
        if (!error.isEmpty()) return false;
    }
    if (reading) {
        QSet<QString> pairs;
        for (auto it = copySources.cbegin(); it != copySources.cend(); ++it) {
            const auto *copy = static_cast<SymmetryCopyObject *>(state.objects[it.key()].data());
            const auto key = it.value() + ":" + QString::number(copy->axis()->id);
            if (!objectIds.contains(it.value()) || pairs.contains(key)) { root.fail("Некорректная или повторная ссылка симметричной копии."); return false; }
            const auto source = state.objects.value(objectIds.value(it.value()));
            if (!supportsObjectReflection(source.data()) || source->editorLayerId() != copy->editorLayerId()) {
                root.fail("Копия ссылается на недопустимый исходник или другой слой."); return false;
            }
            static_cast<SymmetryCopyObject *>(state.objects[it.key()].data())->bindSource(source);
            pairs.insert(key);
        }
        if (usedAxes.size() != axes.size()) { root.fail("Ось симметрии не содержит копий."); return false; }
    }
    if (referencedHardware.size() != hardware.size()) { root.fail("В аппаратной сцене есть объекты без метаданных редактора."); return false; }
    Fields groups(root.child("groups"), reading, error); groups.value("next-id", state.nextGroupId);
    if (reading) {
        state.groups.clear();
        for (auto n = groups.node.firstChildElement("group"); !n.isNull(); n = n.nextSiblingElement("group")) {
            ProjectManager::ObjectGroup group; Fields g(n, true, error); g.value("id", group.id); g.text("name", group.name);
            for (auto member = n.firstChildElement("member"); !member.isNull(); member = member.nextSiblingElement("member")) {
                if (!objectIds.contains(member.attribute("ref"))) root.fail("Группа ссылается на отсутствующий объект.");
                else group.members.append(objectIds.value(member.attribute("ref")));
            }
            state.groups.append(group);
        }
    } else {
        for (auto &group : state.groups) {
            Fields g(append(groups.node, "group"), false, error); g.value("id", group.id); g.text("name", group.name);
            for (int index : group.members) append(g.node, "member").setAttribute("ref", QStringLiteral("object-%1").arg(index + 1));
        }
    }
    QSet<int> groupIds, grouped; int maxGroup = 0;
    for (const auto &group : state.groups) {
        if (group.id <= 0 || group.id == INT_MAX || groupIds.contains(group.id) || group.name.trimmed().isEmpty() || group.members.size() < 2) root.fail("Некорректная группа редактора.");
        groupIds.insert(group.id); maxGroup = qMax(maxGroup, group.id);
        for (int member : group.members) {
            if (member < 0 || member >= state.objects.size() || grouped.contains(member)) root.fail("Некорректный или повторный участник группы.");
            grouped.insert(member);
        }
    }
    if (state.nextGroupId <= maxGroup || state.nextGroupId == INT_MAX) root.fail("Некорректный счётчик групп.");
    // Последний экспорт является состоянием редактора и не попадает в UART/XML objects.
    auto configuration = editor.firstChildElement("fpga-configuration");
    if (reading) {
        state.fpgaConfiguration = {};
        if (!configuration.isNull()) {
            if (!configuration.nextSiblingElement("fpga-configuration").isNull()) root.fail("Повторная конфигурация ПЛИС.");
            for (auto n = configuration.firstChildElement(); !n.isNull(); n = n.nextSiblingElement()) {
                if (n.tagName() != "stage") { root.fail("Неизвестное поле конфигурации ПЛИС."); break; }
                FpgaPipelineStage stage;
                Fields fields(n, true, error);
                fields.value("type", stage.type); fields.value("param-count", stage.parameterCount); fields.value("ramid", stage.ramId);
                state.fpgaConfiguration.stages.append(stage);
            }
            if (!state.fpgaConfiguration.isValid()) root.fail("Повреждена сохранённая конфигурация ПЛИС.");
        }
    } else if (state.fpgaConfiguration.isValid()) {
        configuration = append(editor, "fpga-configuration");
        for (auto stage : state.fpgaConfiguration.stages) {
            Fields fields(append(configuration, "stage"), false, error);
            fields.value("type", stage.type); fields.value("param-count", stage.parameterCount); fields.value("ramid", stage.ramId);
        }
    }
    // Сначала читаемые настройки и сцена, затем потенциально объёмные ресурсы.
    if (!reading) editor.appendChild(resourcesNode);
    return error.isEmpty();
}
}

bool write(QDomDocument &document, const State &source, QString *error)
{
    QString message; State state = source;
    auto editor = document.createElement("editor");
    // Ссылки разрешаются относительно неизменённых аппаратных разделов проекта.
    document.documentElement().appendChild(editor);
    if (!process(editor, state, false, message)) {
        document.documentElement().removeChild(editor);
        if (error) *error = message;
        return false;
    }
    if (error) error->clear();
    return true;
}

bool read(const QDomElement &editor, State *result, QString *error)
{
    QString message; State state;
    if (!result || !process(editor, state, true, message)) { if (error) *error = message; return false; }
    *result = state;
    if (error) error->clear();
    return true;
}
}
