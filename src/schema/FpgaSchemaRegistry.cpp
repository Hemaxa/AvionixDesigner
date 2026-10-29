//FpgaSchemaRegistry - единая точка описания XML/FPGA-схем и UI-каталога объектов

#include "FpgaSchemaRegistry.h"
#include "fpga/model/Layer3dFormat.h"
#include <QDomDocument>
#include <QRegularExpression>
#include <QSet>

namespace {
QList<FpgaSchemaField> layer3dFields()
{
    return {
        {"x", Layer3dFormat::xOffset, Layer3dFormat::coordinateBits},
        {"y", Layer3dFormat::yOffset, Layer3dFormat::coordinateBits},
        {"course", Layer3dFormat::courseOffset, Layer3dFormat::courseBits},
        {"height", Layer3dFormat::heightOffset, Layer3dFormat::heightBits},
        {"yscreen", Layer3dFormat::screenYOffset, Layer3dFormat::screenYBits},
        {"padding", Layer3dFormat::paddingOffset, Layer3dFormat::paddingBits}
    };
}

QList<FpgaSchemaField> rectangleFields()
{
    return {
        {"enb", 0, 1},
        {"color", 1, 24},
        {"colorb", 25, 24},
        {"x0", 49, 12},
        {"y0", 61, 12},
        {"w", 73, 12},
        {"h", 85, 12},
        {"a", 97, 12},
        {"padding", 109, 41}
    };
}

QList<FpgaSchemaField> rectangleAlphaFields()
{
    return {
        {"enb", 0, 1},
        {"color", 1, 24},
        {"colorb", 25, 24},
        {"alph", 49, 3},
        {"x0", 52, 12},
        {"y0", 64, 12},
        {"w", 76, 12},
        {"h", 88, 12},
        {"a", 100, 12},
        {"padding", 112, 38}
    };
}

QList<FpgaSchemaField> rotationObjectFields()
{
    return {
        {"enb", 0, 1},
        {"color", 1, 24},
        {"xrot", 25, 12},
        {"yrot", 37, 12},
        {"top", 49, 12},
        {"left", 61, 12},
        {"bottom", 73, 12},
        {"right", 85, 12},
        {"sq", 97, 8},
        {"sin", 105, 18},
        {"cos", 123, 18},
        {"padding", 141, 9}
    };
}

QList<FpgaSchemaField> staticGroupFields()
{
    return {
        {"enb", 0, 1},
        {"color", 1, 24},
        {"x", 25, 12},
        {"y", 37, 12},
        {"w", 49, 12},
        {"h", 61, 12},
        {"addr", 73, 16},
        {"padding", 89, 61}
    };
}

QList<FpgaSchemaField> aviaHorizonFields()
{
    return {
        {"enb", 0, 1},
        {"earth", 1, 24},
        {"sky", 25, 24},
        {"hline", 49, 24},
        {"width", 73, 4},
        {"xo", 77, 12},
        {"yo", 89, 12},
        {"sn", 101, 18},
        {"cs", 119, 18},
        {"padding", 137, 13}
    };
}

QList<FpgaSchemaField> dashedLineFields()
{
    return {
        {"Enable", 0, 1},
        {"Color", 1, 24},
        {"xo", 25, 12},
        {"yo", 37, 12},
        {"dt", 49, 13},
        {"a", 62, 9},
        {"b", 71, 9},
        {"StepPow", 80, 3},
        {"Length", 83, 8},
        {"Phase", 91, 8},
        {"Width", 99, 4},
        {"padding", 103, 47}
    };
}

QList<FpgaSchemaField> ribbonScaleFields()
{
    return {
        {"enable", 0, 1},
        {"color", 1, 24},
        {"left", 25, 12},
        {"right", 37, 12},
        {"top", 49, 12},
        {"bottom", 61, 12},
        {"width", 73, 4},
        {"period", 77, 8},
        {"ystart", 85, 12},
        {"padding", 97, 53}
    };
}

QList<FpgaSchemaField> fontGlyphFields()
{
    return {
        {"enb", 0, 1},
        {"code", 1, 16},
        {"w", 17, 12},
        {"h", 29, 12},
        {"advance", 41, 12},
        {"bearing_x", 53, 12},
        {"bearing_y", 65, 12},
        {"ascent", 77, 12},
        {"descent", 89, 12},
        {"offset", 101, 24},
        {"mask_size", 125, 12},
        {"font_index", 137, 8},
        {"padding", 145, 5}
    };
}

QList<FpgaSchemaField> textLineFields()
{
    return {
        {"enb", 0, 1},
        {"color", 1, 24},
        {"x", 25, 12},
        {"y", 37, 12},
        {"font_index", 49, 8},
        {"char_offset", 57, 16},
        {"char_count", 73, 12},
        {"padding", 85, 65}
    };
}
}

FpgaSchemaRegistry* FpgaSchemaRegistry::instance()
{
    static FpgaSchemaRegistry registry;
    return &registry;
}

bool FpgaSchemaRegistry::validateXml(const QDomDocument &document, QString *errorMessage) const
{
    if (errorMessage) errorMessage->clear();
    auto fail = [errorMessage](const QString &message) {
        if (errorMessage) *errorMessage = message;
        return false;
    };
    const auto root = document.documentElement();
    if (root.tagName() != QStringLiteral("project")
        || root.attribute("width").toInt() <= 0 || root.attribute("height").toInt() <= 0)
        return fail(QStringLiteral("Ожидается <project> с положительными width и height."));
    const auto parameters = root.firstChildElement("parameters");
    const auto objects = root.firstChildElement("objects");
    if (parameters.isNull() || objects.isNull())
        return fail(QStringLiteral("В XML необходимы разделы <parameters> и <objects>."));
    for (auto child = root.firstChildElement(); !child.isNull(); child = child.nextSiblingElement()) {
        if (child != parameters && child != objects)
            return fail(QStringLiteral("Недопустимый раздел XML: <%1>.").arg(child.tagName()));
    }

    QSet<QString> schemas;
    for (auto schema = parameters.firstChildElement(); !schema.isNull(); schema = schema.nextSiblingElement()) {
        const QString name = schema.tagName();
        const auto expected = fieldsForSchema(name);
        if (expected.isEmpty() || schemas.contains(name))
            return fail(QStringLiteral("Неизвестная или повторная схема <%1>.").arg(name));
        schemas.insert(name);
        auto field = schema.firstChildElement();
        for (const auto &definition : expected) {
            if (field.isNull() || field.tagName() != definition.name
                || field.attribute("offset").toInt() != definition.offset
                || field.attribute("size").toInt() != definition.size)
                return fail(QStringLiteral("Схема <%1> не соответствует формату: ожидается %2 (offset=%3, size=%4).")
                    .arg(name, definition.name).arg(definition.offset).arg(definition.size));
            field = field.nextSiblingElement();
        }
        if (!field.isNull())
            return fail(QStringLiteral("Лишний параметр в схеме <%1>: %2.").arg(name, field.tagName()));
    }

    const QRegularExpression initPattern(QStringLiteral("^[0-3][0-9A-Fa-f]{37}$"));
    int layerCount = 0;
    for (auto object = objects.firstChildElement(); !object.isNull(); object = object.nextSiblingElement()) {
        const QString name = object.tagName();
        if (!schemas.contains(name))
            return fail(QStringLiteral("Неизвестный тип объекта или отсутствует схема: <%1>.").arg(name));
        if (name == QStringLiteral("layer3d") && ++layerCount > 1)
            return fail(QStringLiteral("В проекте допускается только один layer3d."));
        if (object.firstChildElement("init").isNull())
            return fail(QStringLiteral("У объекта <%1> отсутствует <init>.").arg(name));
        for (auto child = object.firstChildElement(); !child.isNull(); child = child.nextSiblingElement()) {
            const QString tag = child.tagName();
            if (tag == QStringLiteral("init")) {
                if (!initPattern.match(child.text().trimmed()).hasMatch())
                    return fail(QStringLiteral("Некорректное 150-битное слово <init> объекта <%1>.").arg(name));
            } else if (tag == QStringLiteral("data")) {
                if (name != "staticgroup" && name != "rotationobject" && name != "font" && name != "text")
                    return fail(QStringLiteral("Объект <%1> не поддерживает <data>.").arg(name));
            } else if (name != QStringLiteral("font") || tag != QStringLiteral("kerning")) {
                return fail(QStringLiteral("Недопустимый элемент <%1> в объекте <%2>.").arg(tag, name));
            }
        }
    }
    return true;
}

QList<FpgaSchemaField> FpgaSchemaRegistry::fieldsForSchema(const QString &schemaName) const
{
    if (schemaName == "layer3d")
        return layer3dFields();
    if (schemaName == "rectangle")
        return rectangleFields();
    if (schemaName == "rectangle_a")
        return rectangleAlphaFields();
    if (schemaName == "rotationobject")
        return rotationObjectFields();
    if (schemaName == "staticgroup")
        return staticGroupFields();
    if (schemaName == "aviahorizont")
        return aviaHorizonFields();
    if (schemaName == "dashline")
        return dashedLineFields();
    if (schemaName == "RibonScale")
        return ribbonScaleFields();
    if (schemaName == "font")
        return fontGlyphFields();
    if (schemaName == "text")
        return textLineFields();
    return {};
}

ParamSchema FpgaSchemaRegistry::buildSchema(const QString &schemaName) const
{
    ParamSchema schema;
    const auto fields = fieldsForSchema(schemaName);
    for (const auto &field : fields) {
        schema.insert(field.name, {field.offset, field.size});
    }
    return schema;
}

QStringList FpgaSchemaRegistry::orderedSchemaNames() const
{
    return {"layer3d", "rectangle", "rectangle_a", "rotationobject", "staticgroup", "aviahorizont", "dashline", "RibonScale", "font", "text"};
}

QStringList FpgaSchemaRegistry::defaultSchemaNames() const
{
    return orderedSchemaNames();
}

QList<EditorObjectDescriptor> FpgaSchemaRegistry::editorObjectCatalog() const
{
    return {
        {"rectangle", QStringLiteral("Прямоугольник"), QStringLiteral(":/icons/icons/library/rectangle.svg"), true, true},
        {"dashline", QStringLiteral("Штриховая линия"), QStringLiteral(":/icons/icons/library/dashedline.svg"), true, true},
        {"RibonScale", QStringLiteral("Ленточная шкала"), QStringLiteral(":/icons/icons/library/ribonscale.svg"), true, true},
        {"aviahorizont", QStringLiteral("Авиагоризонт"), QStringLiteral(":/icons/icons/library/aviahorizon.svg"), true, true},
        {"layer3d", QStringLiteral("3D вид"), QStringLiteral(":/icons/icons/library/three-d-view.svg"), true, true},
        {"text", QStringLiteral("Текст"), QStringLiteral(":/icons/icons/library/text.svg"), true, true},
        {"staticgroup", QStringLiteral("Статическое изображение"), QStringLiteral(":/icons/icons/library/static-image.svg"), false, false},
        {"rotationobject", QStringLiteral("Вращающееся изображение"), QStringLiteral(":/icons/icons/library/rotating-image.svg"), false, false}
    };
}
