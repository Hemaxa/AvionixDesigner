#include "FpgaSchemaRegistry.h"

namespace {
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

QString FpgaSchemaRegistry::canonicalSchemaName(const QString &typeName) const
{
    if (typeName == "rectangle_a")
        return QStringLiteral("rectangle");
    if (typeName == "aviahorizont")
        return QStringLiteral("aviagorizont");
    if (typeName == "text")
        return QStringLiteral("staticgroup");
    if (typeName == "ribonscale")
        return QStringLiteral("RibonScale");
    return typeName;
}

QString FpgaSchemaRegistry::canonicalObjectTag(const QString &typeName) const
{
    if (typeName == "rectangle_a")
        return QStringLiteral("rectangle");
    if (typeName == "aviahorizont")
        return QStringLiteral("aviagorizont");
    if (typeName == "text")
        return QStringLiteral("staticgroup");
    if (typeName == "ribonscale")
        return QStringLiteral("RibonScale");
    return typeName;
}

QList<FpgaSchemaField> FpgaSchemaRegistry::fieldsForSchema(const QString &schemaName) const
{
    if (schemaName == "rectangle")
        return rectangleFields();
    if (schemaName == "rotationobject")
        return rotationObjectFields();
    if (schemaName == "staticgroup")
        return staticGroupFields();
    if (schemaName == "aviagorizont")
        return aviaHorizonFields();
    if (schemaName == "dashed_line")
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
    return {"rectangle", "rotationobject", "staticgroup", "aviagorizont", "dashed_line", "RibonScale", "font", "text"};
}

QStringList FpgaSchemaRegistry::defaultSchemaNames() const
{
    return orderedSchemaNames();
}

QMap<QString, QString> FpgaSchemaRegistry::defaultSchemaAliases() const
{
    return {
        {"rectangle_a", "rectangle"},
        {"aviahorizont", "aviagorizont"},
        {"ribonscale", "RibonScale"}
    };
}

QList<EditorObjectDescriptor> FpgaSchemaRegistry::editorObjectCatalog() const
{
    return {
        {"rectangle", QStringLiteral("Прямоугольник"), QStringLiteral(":/icons/icons/library/rectangle.svg"), true, true},
        {"dashed_line", QStringLiteral("Штриховая линия"), QStringLiteral(":/icons/icons/library/dashedline.svg"), true, true},
        {"ribonscale", QStringLiteral("Ленточная шкала"), QStringLiteral(":/icons/icons/library/ribonscale.svg"), true, true},
        {"aviagorizont", QStringLiteral("Авиагоризонт"), QStringLiteral(":/icons/icons/library/aviahorizon.svg"), true, true},
        {"text", QStringLiteral("Текст"), QStringLiteral(":/icons/icons/library/text.svg"), true, true},
        {"staticgroup", QStringLiteral("Static"), QStringLiteral(":/icons/icons/library/staticgroup.svg"), false, false},
        {"rotationobject", QStringLiteral("Rotation Group"), QStringLiteral(":/icons/icons/library/rotationgroup.svg"), false, false}
    };
}
