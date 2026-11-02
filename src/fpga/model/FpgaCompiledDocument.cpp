// FpgaCompiledDocument - представление XML до расчёта аппаратной сцены.

#include "fpga/model/FpgaCompiledDocument.h"

#include <QDomDocument>
#include "FpgaSchemaRegistry.h"
#include "BitParser.h"
#include "text/FontAtlasXml.h"
#include "text/TextLayout.h"
#include <QDomElement>
#include <QRegularExpression>

namespace {
QString cleanDensePayload(QString text)
{
    text.remove(QLatin1Char('\t'));
    text.remove(QLatin1Char('\n'));
    text.remove(QLatin1Char('\r'));
    text.remove(QLatin1Char(' '));
    text.remove(QLatin1Char(','));
    return text;
}

QString cleanCodePayload(const QString &text)
{
    QStringList codes;
    const QStringList parts = text.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString code = part.trimmed();
        if (!code.isEmpty())
            codes.append(code);
    }
    return codes.join(QLatin1Char(','));
}

QColor colorFromBgrAttribute(const QString &value)
{
    QString text = value.trimmed();
    if (text.startsWith(QLatin1Char('#')))
        text.remove(0, 1);

    bool ok = false;
    const uint packed = text.toUInt(&ok, 16);
    if (!ok)
        return Qt::black;

    return QColor(
        static_cast<int>(packed & 0xFF),
        static_cast<int>((packed >> 8) & 0xFF),
        static_cast<int>((packed >> 16) & 0xFF)
    );
}

QMap<QString, ParamSchema> parseSchemas(const QDomElement &root)
{
    QMap<QString, ParamSchema> schemas;
    const QDomElement paramsEl = root.firstChildElement(QStringLiteral("parameters"));
    QDomElement schemaEl = paramsEl.firstChildElement();
    while (!schemaEl.isNull()) {
        ParamSchema schema;
        QDomElement fieldEl = schemaEl.firstChildElement();
        while (!fieldEl.isNull()) {
            schema.insert(fieldEl.tagName(), {
                fieldEl.attribute(QStringLiteral("offset"), QStringLiteral("0")).toInt(),
                fieldEl.attribute(QStringLiteral("size"), QStringLiteral("0")).toInt()
            });
            fieldEl = fieldEl.nextSiblingElement();
        }
        schemas.insert(schemaEl.tagName(), schema);
        schemaEl = schemaEl.nextSiblingElement();
    }
    return schemas;
}
}

FpgaCompiledDocument FpgaCompiledDocument::fromDomDocument(const QDomDocument &dom, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    FpgaCompiledDocument compiled;
    if (!FpgaSchemaRegistry::instance()->validateXml(dom, errorMessage))
        return compiled;
    const QDomElement root = dom.documentElement();
    if (root.isNull() || root.tagName() != QStringLiteral("project")) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Compiled XML does not contain a <project> root element.");
        return compiled;
    }

    compiled.projectName = root.attribute(QStringLiteral("name"), QStringLiteral("Untitled"));
    compiled.width = root.attribute(QStringLiteral("width"), QStringLiteral("0")).toInt();
    compiled.height = root.attribute(QStringLiteral("height"), QStringLiteral("0")).toInt();
    compiled.backgroundColor = colorFromBgrAttribute(root.attribute(QStringLiteral("bgcolor"), QStringLiteral("#000000")));
    compiled.schemas = parseSchemas(root);

    const QDomElement objectsEl = root.firstChildElement(QStringLiteral("objects"));
    for (auto fontEl = objectsEl.firstChildElement("font"); !fontEl.isNull(); fontEl = fontEl.nextSiblingElement("font")) {
        FpgaFont font;
        if (!readFontAtlas(fontEl, compiled.schemas.value("font"), &font, errorMessage))
            return {};
        if (compiled.fonts.contains(font.index)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Повторный индекс ресурса шрифта: %1.").arg(font.index);
            return {};
        }
        compiled.fonts.insert(font.index, font);
    }
    QDomElement objectEl = objectsEl.firstChildElement();
    while (!objectEl.isNull()) {
        if (objectEl.tagName() == QStringLiteral("font")) {
            objectEl = objectEl.nextSiblingElement();
            continue;
        }
        FpgaCompiledObject object;
        object.type = objectEl.tagName();
        QDomElement childEl = objectEl.firstChildElement();
        while (!childEl.isNull()) {
            if (childEl.tagName() == QStringLiteral("init")) {
                object.iparams.append(cleanDensePayload(childEl.text()));
            } else if (childEl.tagName() == QStringLiteral("data")) {
                object.data = object.type == QStringLiteral("text")
                    ? cleanCodePayload(childEl.text())
                    : cleanDensePayload(childEl.text());
            }
            childEl = childEl.nextSiblingElement();
        }

        if (object.iparams.isEmpty()) {
            compiled.warnings.append(QStringLiteral("Object <%1> has no <init> parameters and was kept without parameter packets.")
                .arg(object.type));
        }
        if ((object.type == "staticgroup" || object.type == "rotationobject") && object.data.isEmpty()) {
            compiled.warnings.append(QStringLiteral("Object <%1> has memory kind but empty <data> section.")
                .arg(object.type));
        }

        if (object.type == QStringLiteral("text")) {
            QString characters;
            for (const auto &code : object.data.split(',', Qt::SkipEmptyParts)) {
                bool ok = false;
                const uint value = code.toUInt(&ok);
                if (!ok || value > 65535) {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("В <text>/<data> ожидаются десятичные коды UTF-16 0…65535.");
                    return {};
                }
                characters.append(QChar(static_cast<ushort>(value)));
            }
            const auto schema = compiled.schemas.value("text");
            for (const auto &hex : object.iparams) {
                auto field = [&schema, &hex](const QString &name) {
                    const auto info = schema.value(name);
                    return int(BitParser::extract(hex, info.offset, info.size));
                };
                const int offset = field("char_offset");
                const int count = field("char_count");
                if (offset + count > characters.size()) {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("char_offset/char_count текста выходят за границы <data>.");
                    return {};
                }
                if (count == 0)
                    continue;
                if (!compiled.fonts.contains(field("font_index"))) {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("Текст ссылается на отсутствующий шрифт %1.").arg(field("font_index"));
                    return {};
                }
                TextLayout layout;
                if (!layoutText(characters.mid(offset, count), compiled.fonts.value(field("font_index")), &layout, errorMessage))
                    return {};
            }
        }

        compiled.objects.append(object);
        objectEl = objectEl.nextSiblingElement();
    }

    if (!compiled.isValid() && errorMessage)
        *errorMessage = QStringLiteral("Compiled XML has invalid project dimensions.");

    return compiled;
}
