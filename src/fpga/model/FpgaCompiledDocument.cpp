//FpgaCompiledDocument - типизированное представление строгого XML, готового к загрузке в ПЛИС

#include "fpga/model/FpgaCompiledDocument.h"

#include <QDomDocument>
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

QString canonicalCompiledName(const QString &name)
{
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("rotation_object"))
        return QStringLiteral("rotationobject");
    if (normalized == QStringLiteral("static_group"))
        return QStringLiteral("staticgroup");
    if (normalized == QStringLiteral("ribonscale"))
        return QStringLiteral("RibonScale");
    if (normalized == QStringLiteral("aviahorizont"))
        return QStringLiteral("aviagorizont");
    return name;
}

FpgaMemoryKind memoryKindForObject(const QString &type)
{
    if (type == QStringLiteral("rotationobject"))
        return FpgaMemoryKind::RotationTiles;
    if (type == QStringLiteral("staticgroup") || type == QStringLiteral("font"))
        return FpgaMemoryKind::StaticMask;
    return FpgaMemoryKind::None;
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
        schemas.insert(canonicalCompiledName(schemaEl.tagName()), schema);
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

    int nextParamIndex = 0;
    int nextMemoryId = 0;
    const QDomElement objectsEl = root.firstChildElement(QStringLiteral("objects"));
    QDomElement objectEl = objectsEl.firstChildElement();
    while (!objectEl.isNull()) {
        FpgaCompiledObject object;
        object.type = canonicalCompiledName(objectEl.tagName());

        QDomElement childEl = objectEl.firstChildElement();
        while (!childEl.isNull()) {
            if (childEl.tagName() == QStringLiteral("init")) {
                object.iparams.append(cleanDensePayload(childEl.text()));
            } else if (childEl.tagName() == QStringLiteral("data")) {
                object.data = object.type == QStringLiteral("text")
                    ? cleanCodePayload(childEl.text())
                    : cleanDensePayload(childEl.text());
            } else if (object.type == QStringLiteral("font") && childEl.tagName() == QStringLiteral("kerning")) {
                const QString left = childEl.attribute(QStringLiteral("left"));
                const QString right = childEl.attribute(QStringLiteral("right"));
                if (!left.isEmpty() && !right.isEmpty()) {
                    object.kerningPairs.insert(
                        left.left(1) + right.left(1),
                        childEl.attribute(QStringLiteral("delta"), QStringLiteral("0")).toInt());
                }
            }
            childEl = childEl.nextSiblingElement();
        }

        object.paramCount = object.iparams.size();
        object.startParamIndex = nextParamIndex;
        nextParamIndex += object.paramCount;

        object.memoryKind = memoryKindForObject(object.type);
        if (object.memoryKind != FpgaMemoryKind::None) {
            object.memId = nextMemoryId;
            ++nextMemoryId;
        }

        if (object.paramCount == 0) {
            compiled.warnings.append(QStringLiteral("Object <%1> has no <init> parameters and was kept without parameter packets.")
                .arg(object.type));
        }
        if (object.memoryKind != FpgaMemoryKind::None && object.data.isEmpty()) {
            compiled.warnings.append(QStringLiteral("Object <%1> has memory kind but empty <data> section.")
                .arg(object.type));
        }

        compiled.objects.append(object);
        objectEl = objectEl.nextSiblingElement();
    }

    if (!compiled.isValid() && errorMessage)
        *errorMessage = QStringLiteral("Compiled XML has invalid project dimensions.");

    return compiled;
}
